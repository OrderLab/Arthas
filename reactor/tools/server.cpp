// The Arthas Project
//
// Copyright (c) 2019, Johns Hopkins University - Order Lab.
//
//    All rights reserved.
//    Licensed under the Apache License, Version 2.0 (the "License");
//

#include "core.h"

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "reactor.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using reactor::ReactRequest;
using reactor::ReactReply;
using reactor::ArthasReactor;

using namespace std;
using namespace llvm;
using namespace arthas;
using namespace llvm::slicing;
using namespace llvm::pmem;
using namespace llvm::instrument;
using namespace llvm::defuse;
using namespace llvm::matching;

// Logic and data behind the server's behavior.
class ReactorServiceImpl final : public ArthasReactor::Service {
 public:
  ReactorServiceImpl(int argc, char* argv[]) {
    reactor = make_unique<Reactor>(make_unique<LLVMContext>());
    this->argc = argc;
    this->argv = argv;
    ready = false;
    background_thd = std::thread(&ReactorServiceImpl::do_background_job, this);
  }

  bool do_background_job() {
    // This is a server mode
    if (!reactor->prepare(argc, argv, true)) {
      cerr << "Failed to prepare reactor, abort\n";
      exit(1);
    }
    cout << "Done with preparation work for reactor, ready to serve\n";
    ready = true;
    trace_monitor_thd =
        std::thread(&Reactor::monitor_address_trace, reactor.get());
    // dependency graph is time consuming to construct...
    reactor->compute_dependencies();
    cout << "Done with computing the program dependency graph\n";
    reactor->wait_address_trace_ready();
    return true;
  }

  Status react(ServerContext* context, const ReactRequest* request,
               ReactReply* reply) override {
    reactor->wait_address_trace_ready();
    string fault_loc = request->fault_loc();
    string fault_instr = request->fault_instr();
    cout << "Reactor got a request to react to fault instruction "
         << fault_instr << " at " << fault_loc << endl;
    if (!ready) {
      reply->set_success(false);
      reply->set_tries(0);
      cerr << "Reactor is not ready, cannot react\n";
      return Status::CANCELLED;
    }
    reaction_result result;
    if (!reactor->react(fault_loc, fault_instr, &result)) {
      reply->set_success(false);
      reply->set_tries(0);
      cerr << "Reactor failed to mitigate this fault " << fault_instr << endl;
      return Status::OK;
    }
    reply->set_success(true);
    reply->set_tries(result.trials);
    return Status::OK;
  }

 private:
  thread background_thd;
  thread trace_monitor_thd;
  bool ready;
  int argc;
  char** argv;
  unique_ptr<Reactor> reactor;
};

void RunServer(int argc, char** argv) {
  ReactorServiceImpl service(argc, argv);

  std::string server_address("0.0.0.0:50052");
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer(argc, argv);
  return 0;
}

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "reactor.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using reactor::ReactRequest;
using reactor::ReactReply;
using reactor::ArthasReactor;

using namespace std;

class ReactorClient {
 public:
  ReactorClient(std::shared_ptr<Channel> channel)
      : stub_(ArthasReactor::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  bool react(const std::string& fault_loc, const std::string& fault_instr) {
    // Data we are sending to the server.
    ReactRequest request;
    request.set_fault_loc(fault_loc);
    request.set_fault_instr(fault_instr);

    // Container for the data we expect from the server.
    ReactReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->react(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      std::cout << "Reactor tried " << reply.tries() << " times" << std::endl;
      return reply.success();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<ArthasReactor::Stub> stub_;
};

const char* program = "reactor-client";
static int show_help = 0;

static struct option long_options[] = {
    /* These options set a flag. */
    {"help", no_argument, &show_help, 1},
    {"server", required_argument, 0, 's'},
    {"fault-inst", required_argument, 0, 'i'},
    {"fault-loc", required_argument, 0, 'c'},
    {0, 0, 0, 0}};

void usage() {
  fprintf(
      stderr,
      "Usage: %s [-h] [OPTION] \n\n"
      "Options:\n"
      "  -h, --help                   : show this help\n"
      "  -s  --server <addr>          : the reactor server address\n"
      "  -i  --fault-inst <string>    : the fault instruction\n"
      "  -c  --fault-loc  <file:line\n"
      "                   [:func]>    : location of the fault instruction \n"
      "\n\n",
      program);
}

struct client_options {
  string server_address;
  string fault_loc;
  string fault_instr;
};

struct client_options options;

bool parse_args(int argc, char** argv) {
  program = argv[0];
  int option_index = 0;
  int c;
  while ((c = getopt_long(argc, argv, "hs:i:c:", long_options,
                          &option_index)) != -1) {
    switch (c) {
      case 'h':
        show_help = 1;
        break;
      case 'c':
        options.fault_loc = optarg;
        break;
      case 'i':
        options.fault_instr = optarg;
        break;
      case 's':
        options.server_address = optarg;
        break;
      case 0:
        break;
      case '?':
      default:
        return false;
    }
  }
  if (show_help) {
    usage();
    exit(0);
  }
  return true;
}

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint specified by
  // the argument "--server".
  if (!parse_args(argc, argv)) {
    usage();
    exit(1);
  }
  if (options.server_address.empty()) {
    // if server address is not specified, we use the default localhost port
    options.server_address = "localhost:50051";
  }
  cout << "Connecting to the reactor server " << options.server_address << "\n";
  ReactorClient reactor(grpc::CreateChannel(
      options.server_address, grpc::InsecureChannelCredentials()));
  bool success = reactor.react(options.fault_loc, options.fault_instr);
  std::cout << "Received reactor reply: " << success << std::endl;
  return 0;
}

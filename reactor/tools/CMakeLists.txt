add_executable(reactor2 
  ../lib/reactor-opts.cpp
  reactor2.cpp
)


add_executable(reactor 
  main.cpp
)

target_link_libraries(reactor
  PUBLIC reactor_core
)

grpc_generate_cpp_src(REACTOR_PROTO_SRCS REACTOR_PROTO_HDRS REACTOR_GRPC_SRCS 
  REACTOR_GRPC_HDRS ${REACTOR_PROTO_GEN_DIR} ${REACTOR_PROTOS})
message(STATUS "Reactor GRPC sources: ${REACTOR_GRPC_SRCS}")

add_executable(reactor_client
  client.cpp
  ${REACTOR_PROTO_SRCS}
  ${REACTOR_GRPC_SRCS}
)
target_link_libraries(reactor_client
  ${GRPC_REFLECTION}
  ${GRPC_CPP}
  ${PROTOBUF_LIBPROTOBUF}
)

add_executable(reactor_server 
  server.cpp
  ${REACTOR_PROTO_SRCS}
  ${REACTOR_GRPC_SRCS}
)

target_link_libraries(reactor_server
  PUBLIC reactor_core
  ${GRPC_REFLECTION}
  ${GRPC_CPP}
  ${PROTOBUF_LIBPROTOBUF}
)

target_link_libraries(reactor2
  PUBLIC checkpoint
  PUBLIC rollback
  PRIVATE Instrumenter
  PRIVATE Slicer
  PRIVATE Matcher
  PRIVATE Utils
  PRIVATE ${PMEM_LIBRARIES}
  PRIVATE pthread 
  PRIVATE dl
  PRIVATE Matcher
  PRIVATE DefUse
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_core}
)

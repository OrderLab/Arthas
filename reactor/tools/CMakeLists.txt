add_executable(reactor 
  reactor-opts.cpp
  reactor.cpp
)
target_link_libraries(reactor
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

grpc_generate_cpp_src(REACTOR_PROTO_SRCS REACTOR_PROTO_HDRS REACTOR_GRPC_SRCS 
  REACTOR_GRPC_HDRS ${REACTOR_PROTO_GEN_DIR} ${REACTOR_PROTOS})
message(STATUS "Reactor GRPC sources: ${REACTOR_GRPC_SRCS}")

foreach(_target
    client server)
  add_executable(reactor_${_target} "${_target}.cpp"
    ${REACTOR_PROTO_SRCS}
    ${REACTOR_GRPC_SRCS})
  target_link_libraries(reactor_${_target}
    ${GRPC_REFLECTION}
    ${GRPC_CPP}
    ${PROTOBUF_LIBPROTOBUF})
endforeach()

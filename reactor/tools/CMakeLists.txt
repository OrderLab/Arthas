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
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_core}
)

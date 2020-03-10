add_executable(reactor 
  reactor-opts.cpp
  reactor.cpp
)
target_link_libraries(reactor
  PUBLIC checkpoint
  PUBLIC rollback
  PRIVATE Instrumenter
  PRIVATE Slicer
  PRIVATE ${PMEM_LIBRARIES}
  PRIVATE pthread 
  PRIVATE dl
)

if(FALSE) # fake a block comment

add_executable(reactor 
  reactor-opts.cpp
  reactor.cpp
)
endif()

add_executable(reactor2 
  reactor-opts.cpp
  reactor2.cpp
)
if(FALSE) # fake a block comment

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
endif()
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

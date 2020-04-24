add_library(checkpoint SHARED
  checkpoint.c
)
add_library(rollback SHARED
  rollback.c
)
# the checkpoint and rollback library should be built with C
set_target_properties(checkpoint rollback PROPERTIES LANGUAGE C)
target_link_libraries(rollback
  PRIVATE checkpoint
)

add_library(reactor_core SHARED
  core.cpp
  reactor-opts.cpp
)

target_link_libraries(reactor_core
  PUBLIC checkpoint
  PUBLIC rollback
  PUBLIC Instrumenter
  PUBLIC Slicer
  PUBLIC Matcher
  PUBLIC Utils
  PUBLIC ${PMEM_LIBRARIES}
  PUBLIC pthread 
  PUBLIC dl
  PUBLIC Matcher
  PUBLIC DefUse
  PUBLIC ${llvm_irreader}
  PUBLIC ${llvm_support}
  PUBLIC ${llvm_core}
)

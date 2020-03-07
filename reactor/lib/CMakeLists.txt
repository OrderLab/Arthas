add_library(checkpoint SHARED
  checkpoint.c
)
add_library(rollback SHARED
  rollback.c
)
target_link_libraries(rollback
  PRIVATE checkpoint
)

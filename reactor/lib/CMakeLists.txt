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

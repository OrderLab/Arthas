set(libsrc 
  addr_tracker.c
)

add_library(AddrTracker SHARED ${libsrc})
add_library(AddrTracker-static STATIC ${libsrc})

set_property(TARGET AddrTracker PROPERTY POSITION_INDEPENDENT_CODE TRUE)
set_target_properties(AddrTracker-static PROPERTIES OUTPUT_NAME AddrTracker)

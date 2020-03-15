# we need to add the two definitions in order to compile and use dg
add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)
add_definitions(-DNDEBUG)

include_directories(include)
include_directories(lib)

add_subdirectory(third-party)
# add dg's include to include path 
include_directories(third-party/dg/include)
# add dg's lib to link path
link_directories(third-party/dg/lib)
# add Weisesr slicer's include to include path 
include_directories(third-party/LLVMSlicer/include)
# add Weiser slicer's lib to link path
link_directories(third-party/LLVMSlicer/lib)

add_subdirectory(future)
include_directories(future/include)
link_directories(future/lib)

add_subdirectory(runtime)
link_directories(runtime)

add_subdirectory(lib)
add_subdirectory(tools)

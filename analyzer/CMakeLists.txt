if (DEFINED ENV{PMDK_HOME})
  set(PMDK_HOME "$ENV{PMDK_HOME}")
endif()

if (PMDK_HOME)
  message(STATUS "PMDK Home analyze: ${PMDK_HOME}")
  set(PMEM_LIBRARY_DIRS ${PMDK_HOME}/src/nondebug)
  set(PMEM_LIBRARIES pmemobj pmem)
  set(PMEM_INCLUDE_DIRS ${PMDK_HOME}/src/include)
else()
  message(WARNING "analyze PMDK HOME is not set, use system-wide PMDK")
  find_package(PkgConfig)
  pkg_check_modules(PMEM REQUIRED libpmemobj libpmem)
endif()

message(STATUS "a PMEM include path: ${PMEM_INCLUDE_DIRS}")
message(STATUS "a PMEM library path: ${PMEM_LIBRARY_DIRS}")
message(STATUS "a PMEM libraries: ${PMEM_LIBRARIES}")
link_directories(${PMEM_LIBRARY_DIRS})

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

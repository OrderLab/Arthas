add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)

if (DEFINED ENV{PMDK_HOME})
  set(PMDK_HOME "$ENV{PMDK_HOME}")
endif()

if (PMDK_HOME) 
  message(STATUS "PMDK Home: ${PMDK_HOME}")
  set(PMEM_LIBRARY_DIRS ${PMDK_HOME}/src/nondebug)
  set(PMEM_LIBRARIES pmemobj pmem)
  set(PMEM_INCLUDE_DIRS ${PMDK_HOME}/src/include)
else()
  message(WARNING "PMDK HOME is not set, use system-wide PMDK")
  find_package(PkgConfig)
  pkg_check_modules(PMEM REQUIRED libpmemobj libpmem)
endif()

message(STATUS "PMEM include path: ${PMEM_INCLUDE_DIRS}")
message(STATUS "PMEM library path: ${PMEM_LIBRARY_DIRS}")
message(STATUS "PMEM libraries: ${PMEM_LIBRARIES}")
link_directories(${PMEM_LIBRARY_DIRS})

include_directories(${ROOT_SOURCE_DIR}/analyzer/include)
link_directories(${ROOT_SOURCE_DIR}/analyzer/lib)

include_directories(${ROOT_SOURCE_DIR}/analyzer/third-party/dg/include)
link_directories(third-party/dg/lib)

include_directories(include)
include_directories(lib)

add_subdirectory(lib)
add_subdirectory(tools)

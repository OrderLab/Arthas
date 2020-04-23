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
link_directories(analyzer/lib)

# need this hack here, otherwise #include "dg/llvm/XXX" in 
# the analyzer won't work anymore 
add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)
include_directories(${ROOT_SOURCE_DIR}/analyzer/third-party/dg/include)
link_directories(third-party/dg/lib)

# Generate the RPC C++ header and source files for the idl
add_subdirectory(idl)

if (REACTOR_PROTOS)
  message(STATUS "Reactor protobuf definitions: ${REACTOR_PROTOS}")
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
  include(FindGrpcAndGenSrc)
  set(REACTOR_PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)
  file(MAKE_DIRECTORY ${REACTOR_PROTO_GEN_DIR})
  include_directories(${REACTOR_PROTO_GEN_DIR})
endif()

include_directories(include)
include_directories(lib)

add_subdirectory(lib)
add_subdirectory(tools)

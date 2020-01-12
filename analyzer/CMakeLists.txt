find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# we need to add the two definitions in order to compile and use dg
add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)

include_directories(include)
include_directories(lib)

# add dg's include to include path 
include_directories(third-party/dg/include)
# add dg's lib to link path
link_directories(third-party/dg/lib)

add_subdirectory(lib)
add_subdirectory(tools)
add_subdirectory(third-party)

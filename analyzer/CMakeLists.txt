find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM binaries: ${LLVM_TOOLS_BINARY_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")

include(LLVMConfig)
include(AddLLVM)

message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libraries dir: ${LLVM_LIBRARY_DIRS}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

include_directories(${LLVM_INCLUDE_DIRS})
message(STATUS "Looking for headers in: ${LLVM_INCLUDE_DIRS}")

link_directories(${LLVM_LIBRARY_DIRS})
message(STATUS "Looking for libraries in: ${LLVM_LIBRARY_DIRS}")

add_definitions(${LLVM_DEFINITIONS})

if (${LLVM_PACKAGE_VERSION} VERSION_GREATER "3.4")
	llvm_map_components_to_libnames(llvm_core core)
	llvm_map_components_to_libnames(llvm_irreader irreader)
	llvm_map_components_to_libnames(llvm_bitwriter bitwriter)
	llvm_map_components_to_libnames(llvm_analysis analysis)
	llvm_map_components_to_libnames(llvm_support support)
	llvm_map_components_to_libnames(llvm_transformutils transformutils)
else()
	llvm_map_components_to_libraries(llvm_core core)
	llvm_map_components_to_libraries(llvm_irreader irreader)
	llvm_map_components_to_libraries(llvm_bitwriter bitwriter)
	llvm_map_components_to_libraries(llvm_analysis analysis)
	llvm_map_components_to_libraries(llvm_support support)
endif()

# we need to add the two definitions in order to compile and use dg
add_definitions(-DHAVE_LLVM)
add_definitions(-DENABLE_CFG)

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

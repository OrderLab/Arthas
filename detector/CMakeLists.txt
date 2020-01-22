find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

if (${LLVM_PACKAGE_VERSION} VERSION_GREATER "3.4")
	llvm_map_components_to_libnames(llvm_core core)
	llvm_map_components_to_libnames(llvm_irreader irreader)
	llvm_map_components_to_libnames(llvm_bitwriter bitwriter)
	llvm_map_components_to_libnames(llvm_analysis analysis)
	llvm_map_components_to_libnames(llvm_support support)
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


add_library(Detector MODULE
  Detector.cpp
)

#include_directories(include)
#include_directories(lib)


#add_subdirectory(lib)
#add_subdirectory(tools)
#add_subdirectory(third-party)

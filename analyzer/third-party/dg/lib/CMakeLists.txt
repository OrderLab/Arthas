add_library(PTA SHARED
  analysis/Offset.cpp

	analysis/PointsTo/Pointer.cpp
	analysis/PointsTo/PointerAnalysis.cpp
	analysis/PointsTo/PointerGraphValidator.cpp
	analysis/PointsTo/PointsToSet.cpp
)

add_library(LLVMpta SHARED
	llvm/analysis/PointsTo/PointerGraphValidator.h
	llvm/analysis/PointsTo/PointerGraph.cpp
	llvm/analysis/PointsTo/PointerGraphValidator.cpp
	llvm/analysis/PointsTo/Block.cpp
	llvm/analysis/PointsTo/Interprocedural.cpp
	llvm/analysis/PointsTo/Structure.cpp
	llvm/analysis/PointsTo/Globals.cpp
	llvm/analysis/PointsTo/Constants.cpp
	llvm/analysis/PointsTo/Instructions.cpp
	llvm/analysis/PointsTo/Calls.cpp
	llvm/analysis/PointsTo/Threads.cpp
)

target_link_libraries(LLVMpta PUBLIC PTA)

add_library(dgThreadRegions SHARED "")
include(${CMAKE_CURRENT_SOURCE_DIR}/llvm/analysis/ThreadRegions/CMakeLists.txt)
target_include_directories(dgThreadRegions PUBLIC
  ${CMAKE_SOURCE_DIR}/include/dg/llvm/analysis/ThreadRegions)
target_link_libraries(dgThreadRegions INTERFACE LLVMpta)

add_library(dgControlDependence SHARED "")
include(${CMAKE_CURRENT_SOURCE_DIR}/llvm/analysis/ControlDependence/CMakeLists.txt)
target_link_libraries(dgControlDependence INTERFACE LLVMpta)

add_library(LLVMdg SHARED
	llvm/LLVMDGVerifier.h
	llvm/llvm-utils.h

	llvm/LLVMNode.cpp
	llvm/LLVMDependenceGraph.cpp
	llvm/LLVMDGVerifier.cpp

	llvm/analysis/Dominators/PostDominators.cpp
	llvm/analysis/DefUse/DefUse.cpp
	llvm/analysis/DefUse/DefUse.h
  llvm/analysis/ReachingDefinitions/LLVMRDBuilder.cpp
	llvm/analysis/ReachingDefinitions/LLVMReachingDefinitions.cpp
	llvm/analysis/ReachingDefinitions/LLVMRDBuilder.h

	analysis/ReachingDefinitions/BasicRDMap.cpp
	analysis/ReachingDefinitions/ReachingDefinitions.cpp
)

target_link_libraries(LLVMdg
  PUBLIC LLVMpta
  PUBLIC dgThreadRegions
  PUBLIC dgControlDependence)

add_library(DGAnalysis SHARED
  ${DG_SOURCE_DIR}/include/dg/analysis/Offset.h
	${DG_SOURCE_DIR}/include/dg/ADT/DGContainer.h
	${DG_SOURCE_DIR}/include/dg/ADT/Bitvector.h
	${DG_SOURCE_DIR}/include/dg/ADT/Bits.h
	${DG_SOURCE_DIR}/include/dg/ADT/NumberSet.h

	analysis/Offset.cpp
)

add_library(PTA SHARED
  ${DG_SOURCE_DIR}/include/dg/analysis/SubgraphNode.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/Pointer.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/PointsToSet.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/MemoryObject.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/PointerGraph.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/PointerAnalysis.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/PointerAnalysisFI.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/PointerAnalysisFS.h
	${DG_SOURCE_DIR}/include/dg/analysis/PointsTo/PointerGraphValidator.h

	analysis/PointsTo/Pointer.cpp
	analysis/PointsTo/PointerAnalysis.cpp
	analysis/PointsTo/PointerGraphValidator.cpp
	analysis/PointsTo/PointsToSet.cpp
)
target_link_libraries(PTA PUBLIC DGAnalysis)

add_library(RD SHARED
  ${DG_SOURCE_DIR}/include/dg/analysis/ReachingDefinitions/ReachingDefinitions.h
	${DG_SOURCE_DIR}/include/dg/analysis/ReachingDefinitions/RDMap.h
	${DG_SOURCE_DIR}/include/dg/analysis/ReachingDefinitions/RDNode.h

	analysis/ReachingDefinitions/BasicRDMap.cpp
	analysis/ReachingDefinitions/ReachingDefinitions.cpp
)
target_link_libraries(RD PUBLIC DGAnalysis)

add_library(LLVMpta SHARED
  ${DG_SOURCE_DIR}/include/dg/llvm/analysis/PointsTo/PointerAnalysis.h
	${DG_SOURCE_DIR}/include/dg/llvm/analysis/PointsTo/LLVMPointerAnalysisOptions.h
	${DG_SOURCE_DIR}/include/dg/llvm/analysis/PointsTo/PointerGraph.h

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

add_library(LLVMrd SHARED
	llvm/analysis/ReachingDefinitions/LLVMRDBuilder.cpp
	llvm/analysis/ReachingDefinitions/LLVMReachingDefinitions.cpp

  ${DG_SOURCE_DIR}/include/dg/llvm/analysis/ReachingDefinitions/ReachingDefinitions.h
	llvm/analysis/ReachingDefinitions/LLVMRDBuilder.h
)
target_link_libraries(LLVMrd
			PUBLIC LLVMpta
			PUBLIC RD)

add_library(dgThreadRegions SHARED "")
include(${CMAKE_CURRENT_SOURCE_DIR}/llvm/analysis/ThreadRegions/CMakeLists.txt)
target_include_directories(dgThreadRegions PUBLIC
  ${CMAKE_SOURCE_DIR}/include/dg/llvm/analysis/ThreadRegions)
target_link_libraries(dgThreadRegions INTERFACE LLVMpta)

add_library(dgControlDependence SHARED "")
include(${CMAKE_CURRENT_SOURCE_DIR}/llvm/analysis/ControlDependence/CMakeLists.txt)
target_link_libraries(dgControlDependence INTERFACE LLVMpta)

add_library(LLVMdg SHARED
  ${DG_SOURCE_DIR}/include/dg/BBlock.h
	${DG_SOURCE_DIR}/include/dg/Node.h
	${DG_SOURCE_DIR}/include/dg/DependenceGraph.h
	${DG_SOURCE_DIR}/include/dg/llvm/LLVMNode.h
	${DG_SOURCE_DIR}/include/dg/llvm/LLVMDependenceGraph.h
	${DG_SOURCE_DIR}/include/dg/llvm/LLVMDependenceGraphBuilder.h
	${DG_SOURCE_DIR}/include/dg/llvm/LLVMSlicer.h

	llvm/LLVMDGVerifier.h
	llvm/llvm-utils.h

	llvm/LLVMNode.cpp
	llvm/LLVMDependenceGraph.cpp
	llvm/LLVMDGVerifier.cpp

	llvm/analysis/Dominators/PostDominators.cpp
	llvm/analysis/DefUse/DefUse.cpp
	llvm/analysis/DefUse/DefUse.h
)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
  set_target_properties(LLVMdg PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
  )

  set_target_properties(LLVMpta PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
  )

  set_target_properties(LLVMrd PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
  )
endif(APPLE)

if (APPLE)
  target_link_libraries(dgControlDependence
        PRIVATE LLVMpta 
				PRIVATE ${llvm_support}
        PRIVATE ${llvm_core})
  target_link_libraries(dgThreadRegions
        PRIVATE LLVMpta 
				PRIVATE ${llvm_support}
        PRIVATE ${llvm_core})
	target_link_libraries(LLVMdg
				PUBLIC LLVMpta
				PUBLIC LLVMrd
				PUBLIC dgThreadRegions
				PUBLIC dgControlDependence
				PRIVATE ${llvm_support}
				PRIVATE ${llvm_analysis}
				PRIVATE ${llvm_irreader}
				PRIVATE ${llvm_bitwriter}
				PRIVATE ${llvm_core})
else()
	target_link_libraries(LLVMdg
				PUBLIC LLVMpta
				PUBLIC LLVMrd
				PUBLIC dgThreadRegions
				PUBLIC dgControlDependence
				PRIVATE ${llvm_analysis}
        )
endif(APPLE)

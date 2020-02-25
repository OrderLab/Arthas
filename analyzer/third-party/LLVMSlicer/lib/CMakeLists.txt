add_library(LLVMWeiserSlicer SHARED
	Kleerer.cpp
	ModStats.cpp
	Slicing/FunctionStaticSlicer.cpp
	Slicing/PostDominanceFrontier.cpp
	Slicing/Prepare.cpp
	Slicing/StaticSlicer.cpp
	Callgraph/Callgraph.cpp
	Languages/LLVM.cpp
	Modifies/Modifies.cpp
	PointsTo/PointsTo.cpp
)

if (APPLE)
  target_link_libraries(LLVMWeiserSlicer
      PRIVATE ${llvm_analysis}
			PRIVATE ${llvm_support}
		  PRIVATE ${llvm_irreader}
      PRIVATE ${llvm_bitwriter}
      PRIVATE ${llvm_transformutils}
			PRIVATE ${llvm_core})
endif(APPLE)

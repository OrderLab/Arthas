include_directories(../include)
include_directories(../lib)

message(STATUS "llvm_core is ${llvm_core}")

add_executable(llvm-dg-dump llvm-dg-dump.cpp)
target_link_libraries(llvm-dg-dump
      PRIVATE LLVMdg
      PRIVATE LLVMSlicer
      PRIVATE ${llvm_support}
      PRIVATE ${llvm_analysis}
      PRIVATE ${llvm_irreader}
      PRIVATE ${llvm_bitwriter}
      PRIVATE ${llvm_core})

add_executable(llvm-pta-dump llvm-pta-dump.cpp)
target_link_libraries(llvm-pta-dump PRIVATE LLVMpta)
target_link_libraries(llvm-pta-dump
			PRIVATE ${llvm_core}
			PRIVATE ${llvm_irreader}
			PRIVATE ${llvm_analysis}
			PRIVATE ${llvm_support})

add_library(dgllvmslicer SHARED
	    llvm-slicer-opts.cpp llvm-slicer-opts.h
	    llvm-slicer-utils.cpp llvm-slicer-utils.h
	    llvm-slicer.h)

add_executable(llvm-slicer llvm-slicer.cpp llvm-slicer-crit.cpp)
target_link_libraries(llvm-slicer 
      PRIVATE dgllvmslicer
      PRIVATE LLVMSlicer
			PRIVATE LLVMdg)
      
target_link_libraries(llvm-slicer
			PRIVATE ${llvm_irreader}
			PRIVATE ${llvm_bitwriter}
			PRIVATE ${llvm_analysis}
			PRIVATE ${llvm_support}
			PRIVATE ${llvm_core})

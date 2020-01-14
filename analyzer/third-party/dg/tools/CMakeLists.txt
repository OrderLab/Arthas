include_directories(../include)
include_directories(../lib)

message(STATUS "llvm_core is ${llvm_core}")

add_executable(llvm-dg-dump llvm-dg-dump.cpp)
target_link_libraries(llvm-dg-dump
      PRIVATE LLVMdg
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

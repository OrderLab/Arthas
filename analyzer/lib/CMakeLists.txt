add_library(DefUse SHARED
  DefUse/DefUse.cpp
)

add_library(LLVMSlicer MODULE 
  Slicing/Slicer.cpp
)

target_link_libraries(LLVMSlicer PUBLIC LLVMdg)

add_library(LLVMPMem MODULE 
  PMem/Extractor.cpp
)

target_link_libraries(LLVMPMem 
  PUBLIC DefUse
  PUBLIC LLVMdg
)

add_library(Utils SHARED
  Utils/sample.cpp
  Utils/LLVM.cpp
)


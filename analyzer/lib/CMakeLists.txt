add_library(LLVMSlicer MODULE 
  Slicing/Slicer.cpp
  )

add_library(LLVMPMem MODULE 
  PMem/Extractor.cpp
  )

add_library(Utils SHARED
  Utils/sample.cpp
  )

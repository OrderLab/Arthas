add_library(DefUse SHARED
  DefUse/DefUse.cpp
)

add_library(Utils SHARED
  Utils/String.cpp
  Utils/Path.cpp
)

add_library(LLVMSlicer MODULE 
  Slicing/Slicer.cpp
  Matcher/Scope.cpp
  Matcher/Matcher.cpp
)

target_link_libraries(LLVMSlicer
  PUBLIC DefUse
  PUBLIC Utils
  PUBLIC LLVMdg
)

add_library(LLVMPMem MODULE 
  PMem/Extractor.cpp
)

target_link_libraries(LLVMPMem 
  PUBLIC DefUse
  PUBLIC LLVMdg
)

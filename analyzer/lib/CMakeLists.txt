add_library(DefUse SHARED
  DefUse/DefUse.cpp
)

add_library(Utils SHARED
  Utils/String.cpp
  Utils/Path.cpp
  Utils/LLVM.cpp
)

target_link_libraries(Utils
  PUBLIC ${llvm_libs}
)

add_library(Matcher SHARED
  Matcher/Scope.cpp
  Matcher/Matcher.cpp
)

target_link_libraries(Matcher
  PUBLIC Utils
)

add_library(LLVMPMem SHARED
  PMem/Extractor.cpp
)

target_link_libraries(LLVMPMem 
  PUBLIC DefUse
  PUBLIC LLVMdg
)

add_library(LLVMSlicer SHARED
  Slicing/Slice.cpp
  Slicing/SliceGraph.cpp
  Slicing/Slicer.cpp
)

target_link_libraries(LLVMSlicer
  PUBLIC DefUse
  PUBLIC Utils
  PUBLIC Matcher
  PUBLIC LLVMdg
  PUBLIC LLVMPMem
)


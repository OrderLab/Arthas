add_library(DefUse SHARED
  DefUse/DefUse.cpp
)

add_library(Utils SHARED
  Utils/String.cpp
  Utils/Path.cpp
  Utils/LLVM.cpp
)

target_link_libraries(Utils
  PRIVATE ${llvm_irreader}
)

add_library(Matcher SHARED
  Matcher/Scope.cpp
  Matcher/Matcher.cpp
)

add_library(Instrument SHARED
  Instrument/PmemAddrInstrumenter.cpp
  Instrument/InstrumentPmemAddrPass.cpp
  Instrument/PmemVarGuidMap.cpp
)

target_link_libraries(Instrument
  PUBLIC PMem
  PRIVATE AddrTracker
  PRIVATE ${llvm_transformutils}
)

add_library(PMem SHARED
  PMem/Extractor.cpp
)

target_link_libraries(PMem 
  PUBLIC DefUse
)

add_library(LLVMPMem MODULE
  PMem/PMemVariablePass.cpp
)

target_link_libraries(LLVMPMem 
  PUBLIC PMem
)

add_library(Slicer SHARED
  Slicing/Slice.cpp
  Slicing/SliceGraph.cpp
  Slicing/SliceCriteria.cpp
  Slicing/DgWalk.cpp
  Slicing/Slicer.cpp
)

target_link_libraries(Slicer
  PUBLIC DefUse
  PUBLIC Matcher
  PUBLIC PMem
  PUBLIC Instrument
  PUBLIC Utils
  PUBLIC LLVMdg
)

add_library(LLVMSlicer MODULE
  Slicing/SlicingPass.cpp
)

target_link_libraries(LLVMSlicer
  PUBLIC Slicer
)

if (APPLE)
  set_target_properties(PMem PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
  )
endif(APPLE)

if (APPLE)
  target_link_libraries(PMem
				PRIVATE ${llvm_analysis}
				PRIVATE ${llvm_support}
				PRIVATE ${llvm_core})
  target_link_libraries(LLVMPMem
				PRIVATE ${llvm_analysis}
				PRIVATE ${llvm_support}
				PRIVATE ${llvm_core})
  target_link_libraries(Utils
				PRIVATE ${llvm_support}
				PRIVATE ${llvm_irreader}
				PRIVATE ${llvm_core})
  target_link_libraries(Matcher
				PRIVATE ${llvm_support}
				PRIVATE ${llvm_irreader}
				PRIVATE ${llvm_core})
  target_link_libraries(LLVMSlicer
				PRIVATE ${llvm_support}
				PRIVATE ${llvm_core})
  target_link_libraries(Instrument
				PRIVATE ${llvm_support}
				PRIVATE ${llvm_core})
endif(APPLE)

add_executable(extractor extractor/main.cpp)
target_link_libraries(extractor
  PUBLIC Utils
  PUBLIC PMem
  PUBLIC DefUse
)
target_link_libraries(extractor
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_core}
)

add_executable(locator locator/main.cpp)
target_link_libraries(locator 
  PRIVATE Utils
  PRIVATE Matcher
)
target_link_libraries(locator
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_core}
)

add_executable(instrumentor instrumentor/main.cpp)
target_link_libraries(instrumentor
  PRIVATE Instrument
  PRIVATE AddrTracker
  PRIVATE Utils
  PRIVATE Matcher
)
target_link_libraries(instrumentor
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_core}
)
add_executable(slicer slicer/main.cpp)
target_link_libraries(slicer
  PRIVATE Utils
  PRIVATE Instrument
  PRIVATE AddrTracker
  PRIVATE Matcher
  PRIVATE Slicer
)
target_link_libraries(slicer
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_analysis}
  PRIVATE ${llvm_bitwriter}
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_core}
)

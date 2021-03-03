add_executable(extractor extractor/main.cpp)
target_link_libraries(extractor
  PUBLIC Utils
  PUBLIC PMem
  PUBLIC DefUse
  PUBLIC Slicer
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
  PUBLIC  PMem
  PUBLIC  DefUse
  PUBLIC ${PMEM_LIBRARIES}
  PRIVATE Instrumenter
  PRIVATE AddrTracker
  PRIVATE Utils
  PRIVATE Matcher
  PRIVATE Slicer
)
target_link_libraries(instrumentor
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_core}
  PUBLIC ${PMEM_LIBRARIES}
  PRIVATE ${llvm_bitwriter}
)
add_executable(slicer slicer/main.cpp)
target_link_libraries(slicer
  PRIVATE Utils
  PRIVATE Instrumenter
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
add_executable(analyzer analyzer/main.cpp)
target_link_libraries(analyzer
  PRIVATE PMem
  PRIVATE Utils
  PUBLIC ${PMEM_LIBRARIES}
  PRIVATE Instrumenter
  PRIVATE AddrTracker
  PRIVATE Matcher
  PRIVATE Slicer
)
target_link_libraries(analyzer
  PRIVATE ${llvm_support}
  PRIVATE ${llvm_analysis}
  PUBLIC ${PMEM_LIBRARIES}
  PRIVATE ${llvm_bitwriter}
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_core}
)

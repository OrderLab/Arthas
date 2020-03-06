add_executable(sample sample/main.cpp)

target_link_libraries(sample PRIVATE Utils)
target_link_libraries(sample 
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

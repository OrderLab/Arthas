add_executable(sample sample/main.cpp)

target_link_libraries(sample PRIVATE Utils)
target_link_libraries(sample 
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_core}
)

add_executable(locator locator/main.cpp)

target_link_libraries(locator
  PRIVATE Utils 
  PRIVATE Matcher
  PRIVATE Slicer
  PRIVATE LLVMdg
)

target_link_libraries(locator
  PRIVATE ${llvm_irreader}
  PRIVATE ${llvm_core}
)

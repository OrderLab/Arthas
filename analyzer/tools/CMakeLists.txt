add_executable(sample sample/main.cpp)

target_link_libraries(sample PRIVATE Utils)
target_link_libraries(sample PRIVATE ${llvm_libs})

add_executable(locator locator/main.cpp)
target_link_libraries(locator
  PRIVATE Utils 
  PUBLIC Matcher
  PRIVATE LLVMSlicer
)

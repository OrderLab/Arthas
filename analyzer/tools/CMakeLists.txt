add_executable(sample sample/main.cpp)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(sample PRIVATE Utils)
target_link_libraries(sample PRIVATE ${llvm_libs})

if (DEFINED ENV{PMDK_HOME})
  set(PMDK_HOME "$ENV{PMDK_HOME}")
endif()

if (PMDK_HOME) 
  message(STATUS "PMDK Home: ${PMDK_HOME}")
  set(PMEM_LIBRARY_DIRS ${PMDK_HOME}/src/nondebug)
  set(PMEM_LIBRARIES libpmemobj libpmem)
  set(PMEM_INCLUDE_DIRS ${PMDK_HOME}/src/include)
else()
  message(WARNING "PMDK HOME is not set, use system-wide PMDK")
  find_package(PkgConfig)
  pkg_check_modules(PMEM REQUIRED libpmemobj libpmem)
endif()

message(STATUS "PMEM include path: ${PMEM_INCLUDE_DIRS}")
message(STATUS "PMEM library path: ${PMEM_LIBRARY_DIRS}")
message(STATUS "PMEM libraries: ${PMEM_LIBRARIES}")
link_directories(${PMEM_LIBRARY_DIRS})

include_directories(${ROOT_SOURCE_DIR}/analyzer/include)
link_directories(${ROOT_SOURCE_DIR}/analyzer/lib)

add_executable(reversion reversion.cpp)
target_link_libraries(reversion 
  PRIVATE Slicer
  PRIVATE ${PMEM_LIBRARIES}
  PRIVATE pthread 
  PRIVATE dl
)

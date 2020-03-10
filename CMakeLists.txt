# The Arthas Project
#
# Copyright (c) 2019, Johns Hopkins University - Order Lab.
#
#    All rights reserved.
#    Licensed under the Apache License, Version 2.0 (the "License");
#

cmake_minimum_required(VERSION 3.4.3)
# Disable in-source builds to prevent source tree corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "FATAL: In-source builds are not allowed.
  You should create a build directory for building the tool.")
endif()

project(arthas)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# Must turn off rtti with -fno-rtti
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -g -std=c++11 -Wall -Wextra -Wno-unused-parameter")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Make clang-format into a make target to enforce coding style
# http://mariobadr.com/using-clang-format-to-enforce-style.html
find_program(
  CLANG_FORMAT_EXE
  NAMES "clang-format"
  DOC "Path to clang-format executable"
)
if(NOT CLANG_FORMAT_EXE)
  message(STATUS "clang-format not found.")
else()
  message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
  set(DO_CLANG_FORMAT "${CLANG_FORMAT_EXE}" "-i -style=file")
endif()

file(GLOB_RECURSE FILES_TO_FORMAT
  ${CMAKE_SOURCE_DIR}/analyzer/*.cpp
  ${CMAKE_SOURCE_DIR}/analyzer/*.h
  ${CMAKE_SOURCE_DIR}/reactor/*.cpp
  ${CMAKE_SOURCE_DIR}/reactor/*.c
  ${CMAKE_SOURCE_DIR}/reactor/*.h
)

set(EXCLUDE_DIRS 
  ${CMAKE_SOURCE_DIR}/analyzer/third-party 
  ${CMAKE_SOURCE_DIR}/analyzer/future
  "/CMakeFiles/"
)

foreach (SOURCE_FILE ${FILES_TO_FORMAT})
  foreach (EXCLUDE_DIR ${EXCLUDE_DIRS})
    string (FIND ${SOURCE_FILE} ${EXCLUDE_DIR} EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
      list (REMOVE_ITEM FILES_TO_FORMAT ${SOURCE_FILE})
    endif ()
  endforeach()
endforeach()

if(CLANG_FORMAT_EXE)
  add_custom_target(
    format-all
    COMMENT "Formatting all source files"
    COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${FILES_TO_FORMAT}
  )
  add_custom_target(
    format-check
    COMMENT "Checking code style format of changed source files"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND scripts/clang-format-changed.py --check-only --clang-format-bin ${CLANG_FORMAT_EXE}
  )
  add_custom_target(
    format
    COMMENT "Format code style of changed source files"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND scripts/clang-format-changed.py --in-place --clang-format-bin ${CLANG_FORMAT_EXE}
  )
endif()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM binaries: ${LLVM_TOOLS_BINARY_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")

include(LLVMConfig)
include(AddLLVM)

message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libraries dir: ${LLVM_LIBRARY_DIRS}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

include_directories(${LLVM_INCLUDE_DIRS})
message(STATUS "Looking for headers in: ${LLVM_INCLUDE_DIRS}")

link_directories(${LLVM_LIBRARY_DIRS})
message(STATUS "Looking for libraries in: ${LLVM_LIBRARY_DIRS}")

add_definitions(${LLVM_DEFINITIONS})

if (${LLVM_PACKAGE_VERSION} VERSION_GREATER "3.4")
	llvm_map_components_to_libnames(llvm_core core)
	llvm_map_components_to_libnames(llvm_irreader irreader)
	llvm_map_components_to_libnames(llvm_bitwriter bitwriter)
	llvm_map_components_to_libnames(llvm_analysis analysis)
	llvm_map_components_to_libnames(llvm_support support)
	llvm_map_components_to_libnames(llvm_transformutils transformutils)
else()
	llvm_map_components_to_libraries(llvm_core core)
	llvm_map_components_to_libraries(llvm_irreader irreader)
	llvm_map_components_to_libraries(llvm_bitwriter bitwriter)
	llvm_map_components_to_libraries(llvm_analysis analysis)
	llvm_map_components_to_libraries(llvm_support support)
endif()

set(ROOT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(analyzer)
add_subdirectory(reactor)


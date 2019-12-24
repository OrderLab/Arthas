# The Pmem-Meta Project
#
# Copyright (c) 2019, Johns Hopkins University - Order Lab.
#
#    All rights reserved.
#    Licensed under the Apache License, Version 2.0 (the "License");
#

cmake_minimum_required(VERSION 3.4.3)
project(pmem-meta)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# Must turn off rtti with -fno-rtti
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -g -std=c++11 -Wall -Wextra")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(analyzer)


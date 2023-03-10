cmake_minimum_required(VERSION 3.13)

project(ai)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(hw1 "Build first homework" ON)
option(hw2 "Build second homework" ON)
option(hw3 "Build third homework" ON)
option(hw4 "Build 4th homework" ON)
option(hw5 "Build 5th homework" ON)

add_library(project_options INTERFACE)
add_library(project_warnings INTERFACE)

include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

add_subdirectory(3rdParty)

if (hw1)
    add_subdirectory(w1)
endif()

if (hw2)
    add_subdirectory(w2)
endif()

if (hw3)
    add_subdirectory(w3)
endif()

if (hw4)
    add_subdirectory(w4)
endif()

if (hw5)
    add_subdirectory(w5)
endif()

add_subdirectory(w6)
add_subdirectory(w7)
add_subdirectory(w8)


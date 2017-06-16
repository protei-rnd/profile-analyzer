cmake_minimum_required(VERSION 3.3.2)
project(psc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -msse4.2 -ggdb -g -fno-omit-frame-pointer -O2")

add_definitions(-D_LINUX)

message(STATUS "psc CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")

include_directories("include")

file(GLOB_RECURSE CPP_FILES    "./source/*.cpp" )

file(GLOB_RECURSE H_FILES      "./include/*.h" )

SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_library(psc STATIC ${CPP_FILES} ${H_FILES})

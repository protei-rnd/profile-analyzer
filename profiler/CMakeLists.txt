project(profile_analyzer)
cmake_minimum_required(VERSION 2.8)
add_definitions(-D_LINUX -D_GLIBCXX_USE_NANOSLEEP)

file(GLOB_RECURSE CPP_FILES "source/*.cpp")
file(GLOB_RECURSE H_FILES   "include/*.h")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -std=c++11 -Wall -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

IF (DEFINED ENV{THIRD_PARTY_HOME})
    set(3D_PARTY_HOME $ENV{THIRD_PARTY_HOME})
    message(STATUS "REWRITE 3D_PARTY_HOME:  ${3D_PARTY_HOME}")
ELSE()
    set(3D_PARTY_HOME "${CMAKE_SOURCE_DIR}/../3dparty")
ENDIF ()

message("3D_PARTY_HOME: ${3D_PARTY_HOME}")

set(PSC_DIR "${3D_PARTY_HOME}/psc")
include_directories(include ${PSC_DIR}/include ${3D_PARTY_HOME}/libunwind/build/include ${3D_PARTY_HOME}/xz/build/include)
link_directories(${PSC_DIR} ${3D_PARTY_HOME}/libunwind/build/lib64 ${3D_PARTY_HOME}/libunwind/build/lib ${3D_PARTY_HOME}/xz/build/lib)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    #add_definitions(-D_GLIBCXX_DEBUG)
    message("Debug mode")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message("Release mode")
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

add_executable(${PROJECT_NAME} ${CPP_FILES} ${H_FILES})

target_link_libraries(${PROJECT_NAME}
#    -Wl,--allow-multiple-definition
    -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
    psc
    unwind-x86_64 unwind-ptrace unwind lzma unwind-x86_64)



#set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
#set(LIBRARY_NAME "mega_profiler")

#add_library(${LIBRARY_NAME} STATIC ${CPP_FILES} ${H_FILES})

#target_link_libraries(${LIBRARY_NAME} -Wl,--whole-archive -lpthread -Wl,--no-whole-archive psc profiler)


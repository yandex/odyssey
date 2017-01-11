cmake_minimum_required(VERSION 2.8)

project(soprano)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    set(CMAKE_C_FLAGS "${CMAKE_CXx_FLAGS} -O2")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif()

include_directories("${PROJECT_SOURCE_DIR}/lib")
include_directories("${PROJECT_BINARY_DIR}/lib")

add_subdirectory(lib)

message (STATUS "")
message (STATUS "soprano: PostgreSQL protocol interaction library.")
message (STATUS "")

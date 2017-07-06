cmake_minimum_required(VERSION 2.8)

project(shapito)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -fPIC -g -O2")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -fPIC -g -O0")
endif()

include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_BINARY_DIR}/")

option(BUILD_SHARED "Enable Shared" OFF)

add_subdirectory(sources)

message (STATUS "")
message (STATUS "Shapito: PostgreSQL protocol interaction library.")
message (STATUS "")
message (STATUS "BUILD_SHARED:     ${BUILD_SHARED}")
message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message (STATUS "")

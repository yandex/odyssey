cmake_minimum_required(VERSION 2.8)

project(machinarium)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -fPIC -g -O2")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -fPIC -g -O0")
endif()

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_BINARY_DIR}/src")

add_custom_target(build_libs)
set(mm_libraries "")

# machinarium
include(BuildLIBUV)
build_libuv()
set(mm_libraries ${mm_libraries} ${LIBUV_LIBRARIES})
include_directories(${LIBUV_INCLUDE_DIRS})

option(BUILD_STATIC "Enable Static" OFF)

add_subdirectory(src)

message (STATUS "")
message (STATUS "machinarium: cooperative multitasking library.")
message (STATUS "")
message (STATUS "BUILD_STATIC:     ${BUILD_STATIC}")
message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message (STATUS "")

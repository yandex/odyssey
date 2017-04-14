cmake_minimum_required(VERSION 2.8)

project(machinarium)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "-std=gnu99 -fPIC -g -O2 -D_GNU_SOURCE")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-std=gnu99 -fPIC -g -O0 -D_GNU_SOURCE")
endif()

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_BINARY_DIR}/src")

option(BUILD_STATIC "Enable Static" OFF)

add_custom_target(build_libs)
set(mm_libraries "")

# openssl
find_package(OpenSSL)
if (NOT OPENSSL_FOUND)
endif()
set(mm_libraries "${mm_libraries} ${OPENSSL_LIBRARIES}")
include_directories("${OPENSSL_INCLUDE_DIR}")

# libcoro
include_directories("${PROJECT_SOURCE_DIR}/lib/libcoro")
include_directories("${PROJECT_BINARY_DIR}/lib/libcoro")

add_subdirectory(src)

message (STATUS "")
message (STATUS "machinarium: cooperative multitasking library.")
message (STATUS "")
message (STATUS "BUILD_STATIC:     ${BUILD_STATIC}")
message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message (STATUS "")

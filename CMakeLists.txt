cmake_minimum_required(VERSION 2.8)

project(machinarium)

find_package(Threads REQUIRED)
set(CMAKE_THREAD_PREFER_PTHREAD true)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "-std=gnu99 -fPIC -g -O2 -D_GNU_SOURCE -pthread")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-std=gnu99 -fPIC -g -O0 -D_GNU_SOURCE -pthread")
endif()

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_BINARY_DIR}/src")

option(BUILD_SHARED "Enable SHARED" OFF)
option(BUILD_VALGRIND "Enable VALGRIND" ON)

set(mm_libraries "")

# valgrind
if (BUILD_VALGRIND)
    find_path(VALGRIND_INCLUDE_PATH "valgrind/valgrind.h"
              "/usr/include"
              "/usr/local/include")
    if (${VALGRIND_INCLUDE_PATH} STREQUAL "VALGRIND_INCLUDE_PATH-NOTFOUND")
    else()
        set(HAVE_VALGRIND 1)
    endif()
endif()

# openssl
find_package(OpenSSL)
if (NOT OPENSSL_FOUND)
endif()
set(mm_libraries "${mm_libraries} ${OPENSSL_LIBRARIES}")
include_directories("${OPENSSL_INCLUDE_DIR}")

add_subdirectory(src)

message (STATUS "")
message (STATUS "machinarium: cooperative multitasking library.")
message (STATUS "")
message (STATUS "BUILD_SHARED:     ${BUILD_SHARED}")
message (STATUS "BUILD_VALGRIND:   ${BUILD_VALGRIND}")
message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message (STATUS "")

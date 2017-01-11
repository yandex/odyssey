cmake_minimum_required(VERSION 2.8)

project(machinarium)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

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

set(mm_libraries "")

find_package(LIBUV)
set(mm_libraries ${mm_libraries} ${LIBUV_LIBRARIES})
include_directories(${LIBUV_INCLUDE_DIRS})

add_subdirectory(lib)

message (STATUS "")
message (STATUS "machinarium: cooperative multitasking library.")
message (STATUS "")

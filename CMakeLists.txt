cmake_minimum_required(VERSION 2.8)

project(odissey)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra -g")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    set(CMAKE_C_FLAGS "${CMAKE_CXx_FLAGS} -O2")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif()

add_custom_target(build_libs)
set(od_libraries "")

# machinarium
include(BuildMachinarium)
build_machinarium()
set(od_libraries ${od_libraries} ${MACHINARIUM_LIBRARIES})
include_directories(${MACHINARIUM_INCLUDE_DIRS})

# soprano
include(BuildSoprano)
build_soprano()
set(od_libraries ${od_libraries} ${SOPRANO_LIBRARIES})
include_directories(${SOPRANO_INCLUDE_DIRS})

message (STATUS "")
message (STATUS "odissey: PostgreSQL connection pooler and request router.")
message (STATUS "")

add_subdirectory(core)

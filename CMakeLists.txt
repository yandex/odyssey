cmake_minimum_required(VERSION 2.8)

project(odyssey)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

execute_process(COMMAND git describe --always OUTPUT_VARIABLE OD_VERSION_GIT
                OUTPUT_STRIP_TRAILING_WHITESPACE)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "-std=gnu99 -pedantic -Wall -Wextra -Wstrict-aliasing -g -O2")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-std=gnu99 -pedantic -Wall -Wextra -Wstrict-aliasing -g -O0")
endif()

string(TOLOWER ${CMAKE_BUILD_TYPE} OD_VERSION_BUILD)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

add_custom_target(build_libs)
set(od_libraries "rt")

# debian build
option(BUILD_DEBIAN "Enable Debian Build" OFF)
if (BUILD_DEBIAN)
    include(BuildDebian)
endif()

# openssl
find_package(OpenSSL REQUIRED)
if (NOT OPENSSL_FOUND)
endif()
include_directories(${OPENSSL_INCLUDE_DIR})

# machinarium
include(BuildMachinarium)
build_machinarium()
set(od_libraries ${od_libraries} ${MACHINARIUM_LIBRARIES} ${OPENSSL_LIBRARIES} "dl")
include_directories(${MACHINARIUM_INCLUDE_DIRS})

# shapito
include(BuildShapito)
build_shapito()
set(od_libraries ${od_libraries} ${SHAPITO_LIBRARIES})
include_directories(${SHAPITO_INCLUDE_DIRS})

message (STATUS "")
message (STATUS "Odyssey (version: ${OD_VERSION_GIT} ${OD_VERSION_BUILD})")
message (STATUS "")
message (STATUS "BUILD_DEBIAN:        ${BUILD_DEBIAN}")
message (STATUS "CMAKE_BUILD_TYPE:    ${CMAKE_BUILD_TYPE}")
message (STATUS "OPENSSL_ROOT_DIR:    ${OPENSSL_ROOT_DIR}")
message (STATUS "OPENSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
message (STATUS "")

add_subdirectory(sources)
add_subdirectory(stress)
add_subdirectory(test)

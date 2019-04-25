cmake_minimum_required(VERSION 3.5)
project(odyssey LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

execute_process(COMMAND git describe --always
                OUTPUT_VARIABLE OD_VERSION_GIT
                RESULT_VARIABLE OD_VERSION_GIT_RESULT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)

if(NOT "${OD_VERSION_GIT_RESULT}" STREQUAL "0")
    set(OD_VERSION_GIT "unknown")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
endif()
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "-std=gnu99 -pedantic -Wall -Wextra -Wstrict-aliasing -g -O2")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-std=gnu99 -pedantic -Wall -Wextra -Wstrict-aliasing -g3 -O0")
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

# use BoringSSL or OpenSSL
option(USE_BORINGSSL "Use BoringSSL" OFF)
if (USE_BORINGSSL)
    find_package(BoringSSL REQUIRED)
    if (BORINGSSL_FOUND)
        set(od_ssl ${BORINGSSL_LIBRARIES})
        include_directories(${BORINGSSL_INCLUDE_DIR})
    endif()
else()
   find_package(OpenSSL REQUIRED)
   set(od_ssl ${OPENSSL_LIBRARIES})
   include_directories(${OPENSSL_INCLUDE_DIR})
endif()

# machinarium
include(BuildMachinarium)
build_machinarium()
set(od_libraries ${od_libraries} ${MACHINARIUM_LIBRARIES} ${od_ssl} "dl")
include_directories(${MACHINARIUM_INCLUDE_DIRS})

# kiwi
include(BuildKiwi)
build_kiwi()
set(od_libraries ${od_libraries} ${KIWI_LIBRARIES})
include_directories(${KIWI_INCLUDE_DIRS})

#memcontext
add_subdirectory(third_party/memcontext)
set(od_libraries ${od_libraries} memcontext)

message (STATUS "")
message (STATUS "Odyssey (version: ${OD_VERSION_GIT} ${OD_VERSION_BUILD})")
message (STATUS "")
message (STATUS "CMAKE_BUILD_TYPE:      ${CMAKE_BUILD_TYPE}")
message (STATUS "BUILD_DEBIAN:          ${BUILD_DEBIAN}")
message (STATUS "USE_BORINGSSL:         ${USE_BORINGSSL}")
message (STATUS "BORINGSSL_ROOT_DIR:    ${BORINGSSL_ROOT_DIR}")
message (STATUS "BORINGSSL_INCLUDE_DIR: ${BORINGSSL_INCLUDE_DIR}")
message (STATUS "OPENSSL_VERSION:       ${OPENSSL_VERSION}")
message (STATUS "OPENSSL_ROOT_DIR:      ${OPENSSL_ROOT_DIR}")
message (STATUS "OPENSSL_INCLUDE_DIR:   ${OPENSSL_INCLUDE_DIR}")
message (STATUS "")

add_subdirectory(sources)
add_subdirectory(test)

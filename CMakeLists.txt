cmake_minimum_required(VERSION 3.16)

project(odyssey C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_C_STANDARD 11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
    file(READ "${CMAKE_SOURCE_DIR}/VERSION" ODYSSEY_VERSION_NUMBER)
    string(STRIP "${ODYSSEY_VERSION_NUMBER}" ODYSSEY_VERSION_NUMBER)
    message(STATUS "Version from VERSION file: ${ODYSSEY_VERSION_NUMBER}")
else()
    set(ODYSSEY_VERSION_NUMBER "unknown")
    message(WARNING "VERSION file not found, using 'unknown'")
endif()

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ODYSSEY_VERSION_GIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(DEFINED ENV{ODYSSEY_VERSION})
    set(ODYSSEY_VERSION_NUMBER $ENV{ODYSSEY_VERSION})
endif()

if(DEFINED ENV{ODYSSEY_GIT_HASH})
    set(ODYSSEY_VERSION_GIT $ENV{ODYSSEY_GIT_HASH})
endif()

if("${ODYSSEY_VERSION_GIT}" STREQUAL "")
    set(ODYSSEY_VERSION_FULL "${ODYSSEY_VERSION_NUMBER}")
else()
    set(ODYSSEY_VERSION_FULL "${ODYSSEY_VERSION_NUMBER}-${ODYSSEY_VERSION_GIT}")
endif()

message(STATUS "")
message(STATUS "Odyssey version configuration:")
message(STATUS "  Version number: ${ODYSSEY_VERSION_NUMBER}")
message(STATUS "  Git hash:       ${ODYSSEY_VERSION_GIT}")
message(STATUS "  Full version:   ${ODYSSEY_VERSION_FULL}")
message(STATUS "")

execute_process(
    COMMAND ${CMAKE_C_COMPILER} --version
    OUTPUT_VARIABLE COMPILER_VERSION_FULL
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

string(REGEX REPLACE "\n.*" "" COMPILER_VERSION_FULL "${COMPILER_VERSION_FULL}")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_BITS "64-bit")
else()
    set(BUILD_BITS "32-bit")
endif()

set(ODYSSEY_COMPILER_STRING "${COMPILER_VERSION_FULL}, ${BUILD_BITS}")

set(OD_DEVEL_LVL -1)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug")
    set(OD_DEVEL_LVL 1)
endif()
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra -Werror -Wstrict-aliasing -g -O2 -pthread")
    set(OD_DEVEL_LVL -1)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDbgInfo")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra -Werror -Wstrict-aliasing -g -O2 -fno-omit-frame-pointer -pthread")
    set(OD_DEVEL_LVL -1)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra -Werror -Wstrict-aliasing -g -O0 -pthread")
    set(OD_DEVEL_LVL 1)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "ASAN")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address,undefined,leak -fno-sanitize-recover=all -fno-omit-frame-pointer -pedantic -Werror -Wall -Wextra -Wstrict-aliasing -g -O0 -pthread")
    set(HAVE_ASAN 1)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "TSAN")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=thread -fno-sanitize-recover=all -fno-omit-frame-pointer -pedantic -Wall -Werror -Wextra -Wstrict-aliasing -g -O0 -pthread")
    set(HAVE_TSAN 1)
else()
    message(FATAL_ERROR "Unknown build type ${CMAKE_BUILD_TYPE}")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" ODYSSEY_BUILD_TYPE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CC_FLAGS}")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

add_custom_target(build_libs)
set(od_libraries "rt" "dl")

set(OPENSSL_INCLUDE_DIR $ENV{OPENSSL_INCLUDE_DIR})
set(OPENSSL_LIBRARIES $ENV{OPENSSL_LIBRARIES})
set(OPENSSL_VERSION $ENV{OPENSSL_VERSION})

# use BoringSSL or OpenSSL
option(USE_BORINGSSL "Use BoringSSL" OFF)
if (USE_BORINGSSL)
    find_package(BoringSSL REQUIRED)
    if (BORINGSSL_FOUND)
        set(od_ssl ${BORINGSSL_LIBRARIES})
        include_directories(${BORINGSSL_INCLUDE_DIR})
    endif ()
elseif ("${OPENSSL_LIBRARIES}" STREQUAL "")
    find_package(OpenSSL REQUIRED)
    set(od_ssl ${OPENSSL_LIBRARIES})
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DUSE_SSL)
else ()	
    set(od_ssl ${OPENSSL_LIBRARIES})
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DUSE_SSL)
endif ()

# use PAM
find_package(PAM)
if (PAM_FOUND)
    set(od_pam ${PAM_LIBRARY})
    include_directories(${PAM_INCLUDE_DIR})
endif()

set(od_libraries ${od_libraries} ${od_pam})

# use LDAP
find_package(LDAP)
if (LDAP_FOUND)
    set(od_ldap ${LDAP_LIBRARY})
    include_directories(${LDAP_INCLUDE_DIR})
endif()

set(od_libraries ${od_libraries} ${od_ldap})

# use systemd (for sd_notify support)
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(SYSTEMD libsystemd)
    if (SYSTEMD_FOUND)
        set(od_systemd ${SYSTEMD_LIBRARIES})
        include_directories(${SYSTEMD_INCLUDE_DIRS})
        link_directories(${SYSTEMD_LIBRARY_DIRS})
        add_definitions(-DHAVE_SYSTEMD)
    endif()
endif()

set(od_libraries ${od_libraries} ${od_systemd})

# use Prom
find_package(Prom)
find_package(Promhttp)

if (PROM_FOUND AND PROMHTTP_FOUND)
    set(od_prom ${PROM_LIBRARY})
    include_directories(${PROM_INCLUDE_DIR})
    set(od_promhttp ${PROMHTTP_LIBRARY})
    include_directories(${PROMHTTP_INCLUDE_DIR})
endif()

set(od_libraries ${od_libraries} ${od_prom})
set(od_libraries ${od_libraries} ${od_promhttp})
set(od_libraries ${od_libraries} ${od_ssl})

#compression
option(BUILD_COMPRESSION "Build with compression support" OFF)
if(BUILD_COMPRESSION)
    set(compression_libraries "")
    add_definitions(-DMM_BUILD_COMPRESSION)
    # use zstd
    find_package(Zstd)
    if(ZSTD_FOUND)
        include_directories(${ZSTD_INCLUDE_DIR})
        set(compression_libraries ${compression_libraries} ${ZSTD_LIBRARY})
        add_definitions(-DMM_HAVE_ZSTD)
    endif()

    # use zlib
    find_package(ZLIB)
    if(ZLIB_FOUND)
        include_directories(${ZLIB_INCLUDE_DIRS})
        set(compression_libraries ${compression_libraries} ${ZLIB_LIBRARIES})
        add_definitions(-DMM_HAVE_ZLIB)
    endif()
endif()

message(STATUS "")
message(STATUS "================================================================")
message(STATUS "Odyssey ${ODYSSEY_VERSION_FULL}")
message(STATUS "")
message(STATUS "Build configuration:")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:       ${ODYSSEY_COMPILER_STRING}")
message(STATUS "  C Flags:          ${CMAKE_C_FLAGS}")
message(STATUS "  Platform:         ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  C lang standard:  ${CMAKE_C_STANDARD}")
message(STATUS "")
message(STATUS "BUILD_DEBIAN:           ${BUILD_DEBIAN}")
message(STATUS "USE_BORINGSSL:          ${USE_BORINGSSL}")
message(STATUS "BORINGSSL_ROOT_DIR:     ${BORINGSSL_ROOT_DIR}")
message(STATUS "BORINGSSL_INCLUDE_DIR:  ${BORINGSSL_INCLUDE_DIR}")
message(STATUS "OPENSSL_VERSION:        ${OPENSSL_VERSION}")
message(STATUS "OPENSSL_ROOT_DIR:       ${OPENSSL_ROOT_DIR}")
message(STATUS "OPENSSL_INCLUDE_DIR:    ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OPENSSL_LIBRARIES:      ${OPENSSL_LIBRARIES}")

if (PAM_FOUND)
    message(STATUS "PAM_LIBRARY:            ${PAM_LIBRARY}")
    message(STATUS "PAM_INCLUDE_DIR:        ${PAM_INCLUDE_DIR}")
endif()

message(STATUS "BUILD_COMPRESSION:      ${BUILD_COMPRESSION}")
if (BUILD_COMPRESSION)
    message(STATUS "ZSTD_INCLUDE_DIR:       ${ZSTD_INCLUDE_DIR}")
    message(STATUS "ZSTD_LIBRARY:           ${ZSTD_LIBRARY}")
    message(STATUS "ZLIB_INCLUDE_DIRS:      ${ZLIB_INCLUDE_DIRS}")
    message(STATUS "ZLIB_LIBRARIES:         ${ZLIB_LIBRARIES}")
endif()

    message(STATUS "LDAP_SUPPORT:           ${LDAP_FOUND}")
if (LDAP_FOUND)
    message(STATUS "LDAP_LIBRARY:           ${LDAP_LIBRARY}")
    message(STATUS "LDAP_INCLUDE_DIR:       ${LDAP_INCLUDE_DIR}")
endif()

message(STATUS "SYSTEMD_FOUND:          ${SYSTEMD_FOUND}")
if (SYSTEMD_FOUND)
    message(STATUS "SYSTEMD_LIBRARIES:      ${SYSTEMD_LIBRARIES}")
    message(STATUS "SYSTEMD_INCLUDE_DIRS:   ${SYSTEMD_INCLUDE_DIRS}")
endif()

add_subdirectory(sources)

set(OD_INSTALL_PREFIX "/usr/bin/")

install(TARGETS odyssey DESTINATION "${OD_INSTALL_PREFIX}")

include(Packaging)

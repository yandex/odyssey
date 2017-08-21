cmake_minimum_required(VERSION 2.8)

project(odissey)

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

# machinarium
include(BuildMachinarium)
build_machinarium()
set(od_libraries ${od_libraries} ${MACHINARIUM_LIBRARIES})
include_directories(${MACHINARIUM_INCLUDE_DIRS})

# openssl
find_package(OpenSSL)
if (NOT OPENSSL_FOUND)
endif()
set(od_libraries ${od_libraries} ${OPENSSL_LIBRARIES} "-ldl")
include_directories("${OPENSSL_INCLUDE_DIR}")

# shapito
include(BuildShapito)
build_shapito()
set(od_libraries ${od_libraries} ${SHAPITO_LIBRARIES})
include_directories(${SHAPITO_INCLUDE_DIRS})

message (STATUS "")
message (STATUS "Odissey (version: ${OD_VERSION_GIT} ${OD_VERSION_BUILD})")
message (STATUS "")

add_subdirectory(sources)


# install section
set(NAME ${CMAKE_PROJECT_NAME})
set(DEST_CONFIG_DIR /etc/${NAME})
set(DEST_CONFIG_FILE ${NAME}.conf)
set(DEST_CONFIG_PATH ${DEST_CONFIG_DIR}/${DEST_CONFIG_FILE})
set(DEST_BIN_DIR /usr/bin)
set(DEST_BIN_FILE ${NAME})
set(DEST_BIN_PATH ${DEST_BIN_DIR}/${DEST_BIN_FILE})
set(DEST_INIT_DIR /etc/init.d)
set(DEST_INIT_FILE ${NAME})
set(DEST_LOG_DIR /var/log/${NAME})
set(DEST_LOG_FILE ${NAME}.log)
set(DEST_LOG_PATH ${DEST_LOG_DIR}/${DEST_LOG_FILE})
set(DEST_PID_DIR /var/run/${NAME})
set(DEST_PID_FILE ${NAME}.pid)
set(DEST_PID_PATH ${DEST_PID_DIR}/${DEST_PID_FILE})
set(DEST_LOGROTATE_DIR /etc/logrotate.d)
set(DEST_LOGROTATE_FILE ${NAME})
set(DEST_LOGROTATE_PATH ${DEST_LOGROTATE_DIR}/${DEST_LOGROTATE_PATH})
set(RENDERED_INIT_FILE ${NAME}.init)
set(RENDERED_CONFIG_FILE ${NAME}.conf.example)
set(RENDERED_LOGROTATE_FILE ${NAME}.logrotate)
set(LOCAL_SCRIPTS_DIR scripts/debian)
set(LOCAL_INIT_FILE ${LOCAL_SCRIPTS_DIR}/init)
set(LOCAL_CONFIG_FILE ${LOCAL_SCRIPTS_DIR}/config)
set(LOCAL_LOGROTATE_FILE ${LOCAL_SCRIPTS_DIR}/logrotate)
set(LOCAL_BIN_DIR sources)
set(LOCAL_BIN_FILE ${LOCAL_BIN_DIR}/${NAME})
set(USER postgres)
set(GROUP postgres)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    ${RENDERED_INIT_FILE} ${RENDERED_CONFIG_FILE} ${RENDERED_LOGROTATE_FILE}
    ${DEBIAN_DIR}/rules ${DEBIAN_DIR}/control ${DEBIAN_DIR}/changelog
)

configure_file(${LOCAL_INIT_FILE} ${RENDERED_INIT_FILE} @ONLY)
configure_file(${LOCAL_CONFIG_FILE} ${RENDERED_CONFIG_FILE} @ONLY)
configure_file(${LOCAL_LOGROTATE_FILE} ${RENDERED_LOGROTATE_FILE} @ONLY)

# configure debian package files
set(VERSION $ENV{VERSION})
if ("${VERSION}" STREQUAL "")
    set(VERSION "0.0.1")
endif()
set(BUILD_NUMBER $ENV{BUILD_NUMBER})
if ("${BUILD_NUMBER}" STREQUAL "")
    set(BUILD_NUMBER "1")
endif()
set(DEBIAN_DIR debian)
execute_process(COMMAND whoami OUTPUT_VARIABLE WHOAMI OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND hostname OUTPUT_VARIABLE HOSTNAME OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND date "+%a, %d %b %Y %H:%M:%S %z" OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
configure_file(${LOCAL_SCRIPTS_DIR}/rules ${DEBIAN_DIR}/rules @ONLY)
configure_file(${LOCAL_SCRIPTS_DIR}/control ${DEBIAN_DIR}/control @ONLY)
configure_file(${LOCAL_SCRIPTS_DIR}/changelog ${DEBIAN_DIR}/changelog @ONLY)

install(PROGRAMS ${RENDERED_INIT_FILE} DESTINATION ${DEST_INIT_DIR} RENAME ${DEST_INIT_FILE})
install(PROGRAMS ${LOCAL_BIN_FILE} DESTINATION ${DEST_BIN_DIR} RENAME ${DEST_BIN_FILE})
install(FILES ${RENDERED_CONFIG_FILE} DESTINATION ${DEST_CONFIG_DIR} RENAME ${DEST_CONFIG_FILE})
install(FILES ${RENDERED_LOGROTATE_FILE} DESTINATION ${DEST_LOGROTATE_DIR} RENAME ${DEST_LOGROTATE_FILE})

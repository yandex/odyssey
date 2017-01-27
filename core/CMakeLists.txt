set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

set(od_binary odissey)
set(od_src
    od_pid.c
    od_syslog.c
    od_log.c
    od_daemon.c
    od_lex.c
    od_scheme.c
    od_config.c
    od.c
    od_io.c
    od_client_pool.c
    od_server_pool.c
    od_route_pool.c
    od_pooler.c
    od_router.c
    od_router_session.c
    od_router_transaction.c
    od_cancel.c
    od_auth.c
    od_periodic.c
    od_fe.c
    od_be.c
    odissey.c)

configure_file("od_build.h.cmake" "od_build.h")

include_directories("${PROJECT_SOURCE_DIR}/core")
include_directories("${PROJECT_BINARY_DIR}/core")

add_executable(${od_binary} ${od_src})
add_dependencies(${od_binary} build_libs)

if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET ${od_binary} PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET ${od_binary} PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()

target_link_libraries(${od_binary} ${od_libraries} ${CMAKE_THREAD_LIBS_INIT})


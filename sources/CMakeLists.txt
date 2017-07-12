
set(od_binary odissey)
set(od_src
    pid.c
    syslog.c
    log.c
    daemon.c
    scheme.c
    config.c
    id.c
    instance.c
    server_pool.c
    client_pool.c
    route_pool.c
    io.c
    pooler.c
    router.c
    console.c
    relay.c
    relay_pool.c
    frontend.c
    backend.c
    auth.c
    cancel.c
    periodic.c
    tls.c
    main.c
)

configure_file("version.h.cmake" "version.h")

include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_BINARY_DIR}/")

add_executable(${od_binary} ${od_src})
add_dependencies(${od_binary} build_libs)

if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET ${od_binary} PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET ${od_binary} PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()

target_link_libraries(${od_binary} ${od_libraries} ${CMAKE_THREAD_LIBS_INIT})

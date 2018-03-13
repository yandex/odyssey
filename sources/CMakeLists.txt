
set(od_binary ${CMAKE_PROJECT_NAME})
set(od_src
    pid.c
    daemon.c
    logger.c
    config.c
    config_reader.c
    id.c
    instance.c
    server_pool.c
    client_pool.c
    route_pool.c
    io.c
    system.c
    router.c
    console.c
    worker.c
    worker_pool.c
    frontend.c
    backend.c
    reset.c
    deploy.c
    auth.c
    auth_query.c
    cancel.c
    cron.c
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

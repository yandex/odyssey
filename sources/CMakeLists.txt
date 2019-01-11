
set(od_binary ${CMAKE_PROJECT_NAME})
set(od_src
    daemon.c
    pid.c
    id.c
    logger.c
    rules.c
    config.c
    config_reader.c
    dns.c
    router.c
    system.c
    cron.c
    worker.c
    tls.c
    auth_query.c
    auth.c
    cancel.c
    console.c
    deploy.c
    reset.c
    frontend.c
    backend.c
    instance.c
    main.c
)

configure_file("version.h.cmake" "version.h")

include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_SOURCE_DIR}/sources")
include_directories("${PROJECT_BINARY_DIR}/")
include_directories("${PROJECT_BINARY_DIR}/sources")

add_executable(${od_binary} ${od_src})
add_dependencies(${od_binary} build_libs)

if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET ${od_binary} PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET ${od_binary} PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()

target_link_libraries(${od_binary} ${od_libraries} ${CMAKE_THREAD_LIBS_INIT})

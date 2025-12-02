configure_file("build.h.cmake" "build.h")
configure_file("machinarium/build.h.cmake" "machinarium/build.h")

set(od_binary ${CMAKE_PROJECT_NAME})
set(od_src
    kiwi/md5.c
    kiwi/options.c
    machinarium/thread.c
    machinarium/lrand48.c
    machinarium/loop.c
    machinarium/clock.c
    machinarium/socket.c
    machinarium/stat.c
    machinarium/epoll.c
    machinarium/context_stack.c
    machinarium/context.c
    machinarium/coroutine.c
    machinarium/coroutine_cache.c
    machinarium/scheduler.c
    machinarium/call.c
    machinarium/signal_mgr.c
    machinarium/event_mgr.c
    machinarium/machine.c
    machinarium/mm.c
    machinarium/machine_mgr.c
    machinarium/msg_cache.c
    machinarium/msg.c
    machinarium/mutex.c
    machinarium/memory.c
    machinarium/channel.c
    machinarium/channel_api.c
    machinarium/task_mgr.c
    machinarium/tls.c
    machinarium/io.c
    machinarium/iov.c
    machinarium/close.c
    machinarium/connect.c
    machinarium/bind.c
    machinarium/backtrace.c
    machinarium/eventfd.c
    machinarium/cond.c
    machinarium/read.c
    machinarium/write.c
    machinarium/accept.c
    machinarium/ring_buffer.c
    machinarium/shutdown.c
    machinarium/dns.c
    machinarium/wait_list.c
    machinarium/wait_flag.c
    machinarium/wait_group.c
    machinarium/zpq_stream.c
    machinarium/compression.c
    machinarium/cert_hash.c
    memory.c
    daemon.c
    pid.c
    logger.c
    pool.c
    rules.c
    config.c
    config_reader.c
    dns.c
    router.c
    global.c
    system.c
    cron.c
    worker.c
    tls.c
    attribute.c
    auth_query.c
    auth.c
    cancel.c
    client.c
    relay.c
    console.c
    deploy.c
    reset.c
    frontend.c
    backend_sync.c
    backend.c
    instance.c
    misc.c
    tdigest.c
    module.c
    attach.c
    multi_pool.c
    counter.c
    err_logger.c
    setproctitle.c
    debugprintf.c
    restart_sync.c
    soft_oom.c
    grac_shutdown_worker.c
    sighandler.c
    watchdog.c
    ejection.c
    thread_global.c
    host_watcher.c
    compression.c
    option.c
    tls_config.c
    tsa.c
    query.c
    storage.c
    server.c
    murmurhash.c
    hashmap.c
    address.c
    hba.c
    hba_reader.c
    hba_rule.c
    mdb_iamproxy.c
    external_auth.c
    group.c
    query_processing.c)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set_property(SOURCE machinarium/context_swap_x64.S PROPERTY LANGUAGE C)
    list(APPEND od_src machinarium/context_swap_x64.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|i.86")
    set_property(SOURCE machinarium/context_swap_x32.S PROPERTY LANGUAGE C)
    list(APPEND od_src machinarium/context_swap_x32.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM|aarch64")
    set_property(SOURCE machinarium/context_swap_aarch64.S PROPERTY LANGUAGE C)
    list(APPEND od_src machinarium/context_swap_aarch64.S)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if (PAM_FOUND)
    list(APPEND od_src pam.c)
endif()

if (POSTGRESQL_FOUND)
	list(APPEND od_src scram.c)
endif()

if (LDAP_FOUND)
        list(APPEND od_src ldap.c)
endif()

if (PROM_FOUND)
        list(APPEND od_src prom_metrics.c)
endif()

include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_SOURCE_DIR}/sources")
include_directories("${PROJECT_BINARY_DIR}/")
include_directories("${PROJECT_BINARY_DIR}/sources")

add_executable(${od_binary} ${od_src} main.c)
add_dependencies(${od_binary} build_libs)

if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET ${od_binary} PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET ${od_binary} PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()

target_link_libraries(${od_binary} ${od_libraries} ${CMAKE_THREAD_LIBS_INIT} m)
if(EXISTS "/etc/alpine-release")
    target_link_libraries(${od_binary} argp)
endif()

if (BUILD_COMPRESSION)
    target_link_libraries(${od_binary} ${compression_libraries})
endif()


# Unit test building

set(od_test_binary odyssey_test)
set(od_test_src
    tests/odyssey_test.c
    tests/kiwi/test_kiwi_enquote.c
    tests/kiwi/test_kiwi_pgoptions.c
    tests/machinarium/test_init.c
    tests/machinarium/test_create0.c
    tests/machinarium/test_create1.c
    tests/machinarium/test_config.c
    tests/machinarium/test_context_switch.c
    tests/machinarium/test_sleep.c
    tests/machinarium/test_sleep_yield.c
    tests/machinarium/test_sleep_cancel0.c
    tests/machinarium/test_join.c
    tests/machinarium/test_condition0.c
    tests/machinarium/test_eventfd.c
    tests/machinarium/test_stat.c
    tests/machinarium/test_signal0.c
    tests/machinarium/test_signal1.c
    tests/machinarium/test_signal2.c
    tests/machinarium/test_channel_create.c
    tests/machinarium/test_channel_rw0.c
    tests/machinarium/test_channel_rw1.c
    tests/machinarium/test_channel_rw2.c
    tests/machinarium/test_channel_rw3.c
    tests/machinarium/test_channel_rw4.c
    tests/machinarium/test_channel_timeout.c
    tests/machinarium/test_channel_cancel.c
    tests/machinarium/test_channel_shared_create.c
    tests/machinarium/test_channel_shared_rw0.c
    tests/machinarium/test_channel_shared_rw1.c
    tests/machinarium/test_channel_shared_rw2.c
    tests/machinarium/test_sleeplock.c
    tests/machinarium/test_producer_consumer0.c
    tests/machinarium/test_producer_consumer1.c
    tests/machinarium/test_producer_consumer2.c
    tests/machinarium/test_io_new.c
    tests/machinarium/test_connect.c
    tests/machinarium/test_connect_timeout.c
    tests/machinarium/test_connect_cancel0.c
    tests/machinarium/test_connect_cancel1.c
    tests/machinarium/test_accept_timeout.c
    tests/machinarium/test_accept_cancel.c
    tests/machinarium/test_advice_keepalive_usr_timeout.c
    tests/machinarium/test_wait_list_compare_wait_timeout.c
    tests/machinarium/test_wait_list_notify_after_compare_wait.c
    tests/machinarium/test_wait_list_compare_wait_wrong_value.c
    tests/machinarium/test_wait_list_without_notify.c
    tests/machinarium/test_wait_list_notify_after_wait.c
    tests/machinarium/test_wait_list_one_producer_multiple_consumers.c
    tests/machinarium/test_wait_list_one_producer_multiple_consumers_threads.c
    tests/machinarium/test_wait_list_notify_all.c
    tests/machinarium/test_wait_group_simple.c
    tests/machinarium/test_wait_group_timeout.c
    tests/machinarium/test_wait_group_lifetime.c
    tests/machinarium/test_wait_flag_simple.c
    tests/machinarium/test_wait_flag_timeout.c
    tests/machinarium/test_getaddrinfo0.c
    tests/machinarium/test_getaddrinfo1.c
    tests/machinarium/test_getaddrinfo2.c
    tests/machinarium/test_client_server0.c
    tests/machinarium/test_client_server1.c
    tests/machinarium/test_client_server2.c
    tests/machinarium/test_client_server_unix_socket.c
    tests/machinarium/test_client_server_unix_socket_no_msg.c
    tests/machinarium/test_coroutine_names.c
    tests/machinarium/test_mutex_threads.c
    tests/machinarium/test_mutex_coroutines.c
    tests/machinarium/test_mutex_timeout.c
    tests/machinarium/test_read_10mb0.c
    tests/machinarium/test_read_10mb1.c
    tests/machinarium/test_read_10mb2.c
    tests/machinarium/test_read_timeout.c
    tests/machinarium/test_read_cancel.c
    tests/machinarium/test_read_var.c
    tests/machinarium/test_ring_buffer.c
    tests/machinarium/test_tls0.c
    tests/machinarium/test_tls_unix_socket.c
    tests/machinarium/test_tls_unix_socket_no_msg.c
    tests/machinarium/test_tls_read_10mb0.c
    tests/machinarium/test_tls_read_10mb1.c
    tests/machinarium/test_tls_read_10mb2.c
    tests/machinarium/test_tls_read_multithread.c
    tests/machinarium/test_tls_read_var.c
    tests/odyssey/test_attribute.c
    tests/odyssey/test_tdigest.c
    tests/odyssey/test_util.c
    tests/odyssey/test_locks.c
    tests/odyssey/test_hba_parse.c
    tests/odyssey/test_address.c
    tests/odyssey/test_hashmap.c)

include_directories("${PROJECT_SOURCE_DIR}/tests")
include_directories("${PROJECT_BINARY_DIR}/tests")

file(COPY tests/machinarium/ca.crt DESTINATION machinarium)
file(COPY tests/machinarium/client.crt DESTINATION machinarium)
file(COPY tests/machinarium/client.key DESTINATION machinarium)
file(COPY tests/machinarium/server.crt DESTINATION machinarium)
file(COPY tests/machinarium/server.key DESTINATION machinarium)

add_executable(${od_test_binary} ${od_test_src} ${od_src})
add_dependencies(${od_test_binary} build_libs)

if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET ${od_test_binary} PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET ${od_test_binary} PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()

target_link_libraries(${od_test_binary} ${od_libraries} ${CMAKE_THREAD_LIBS_INIT} m)

if(EXISTS "/etc/alpine-release")
    target_link_libraries(${od_test_binary} argp)
endif()

if (BUILD_COMPRESSION)
    target_link_libraries(${od_test_binary} ${compression_libraries})
endif()

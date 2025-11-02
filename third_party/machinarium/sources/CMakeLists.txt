set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wstrict-aliasing")

set(machine_library machinarium)
set(machine_src
    thread.c
    lrand48.c
    loop.c
    clock.c
    socket.c
    stat.c
    epoll.c
    context_stack.c
    context.c
    coroutine.c
    coroutine_cache.c
    scheduler.c
    call.c
    signal_mgr.c
    event_mgr.c
    machine.c
    mm.c
    machine_mgr.c
    msg_cache.c
    msg.c
    mutex.c
    memory.c
    channel.c
    channel_api.c
    task_mgr.c
    tls.c
    io.c
    iov.c
    close.c
    connect.c
    bind.c
    backtrace.c
    eventfd.c
    cond.c
    read.c
    write.c
    accept.c
    ring_buffer.c
    shutdown.c
    dns.c
    wait_list.c
    wait_flag.c
    wait_group.c
    zpq_stream.c
    compression.c
    cert_hash.c)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set_property(SOURCE context_swap_x64.S PROPERTY LANGUAGE C)
    list(APPEND machine_src context_swap_x64.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|i.86")
    set_property(SOURCE context_swap_x32.S PROPERTY LANGUAGE C)
    list(APPEND machine_src context_swap_x32.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM|aarch64")
    set_property(SOURCE context_swap_aarch64.S PROPERTY LANGUAGE C)
    list(APPEND machine_src context_swap_aarch64.S)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_library(machine_library_static STATIC ${machine_src})
set_target_properties(machine_library_static PROPERTIES OUTPUT_NAME ${machine_library})

if (BUILD_COMPRESSION)
    target_link_libraries(machine_library_static ${compression_libraries})
endif()

if (BUILD_SHARED)
    add_library(machine_library_shared SHARED ${machine_src})
    set_target_properties(machine_library_shared PROPERTIES OUTPUT_NAME ${machine_library})
    target_link_libraries(machine_library_shared ${machine_libraries} m rt)
endif(BUILD_SHARED)

configure_file("build.h.cmake" "build.h")

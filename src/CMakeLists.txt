set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

set(machine_library machinarium)
set(machine_src mm_thread.c
                mm_loop.c
                mm_clock.c
                mm_socket.c
                mm_epoll.c
                mm_context_stack.c
                mm_context.c
                mm_fiber.c
                mm_scheduler.c
                mm_call.c
                mm.c
                mm_machine.c
                mm_machine_mgr.c
                mm_tls.c
                mm_tls_io.c
                mm_io.c
                mm_close.c
                mm_connect.c
                mm_bind.c
                mm_read.c
                mm_write.c
                mm_accept.c
                mm_dns.c)

add_library(machine_library_static STATIC ${machine_src})
set_target_properties(machine_library_static PROPERTIES OUTPUT_NAME ${machine_library})

if (BUILD_SHARED)
  add_library(machine_library_shared SHARED ${machine_src})
  set_target_properties(machine_library_shared PROPERTIES OUTPUT_NAME ${machine_library})
  target_link_libraries(machine_library_shared ${machine_libraries} m rt)
endif(BUILD_SHARED)

configure_file("mm_build.h.cmake" "mm_build.h")

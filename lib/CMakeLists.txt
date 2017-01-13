
set(mm_library machinarium)
set(mm_src
    mm_context.c
    mm_fiber.c
    mm_scheduler.c
    mm.c
    mm_io.c
    mm_getsockname.c
    mm_getpeername.c
    mm_getaddrinfo.c
    mm_connect.c
    mm_bind.c
    mm_accept.c
    mm_read.c
    mm_write.c)

add_library(mm_library_shared SHARED ${mm_src})
set_target_properties(mm_library_shared PROPERTIES OUTPUT_NAME ${mm_library})
target_link_libraries(mm_library_shared ${mm_libraries} m rt)

if (BUILD_STATIC)
  add_library(mm_library_static STATIC ${mm_src})
  set_target_properties(mm_library_static PROPERTIES OUTPUT_NAME ${mm_library})
endif(BUILD_STATIC)

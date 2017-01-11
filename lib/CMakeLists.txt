set(mm_library "machinarium")
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

add_library(${mm_library} SHARED ${mm_src})
target_link_libraries(${mm_library} ${mm_libraries} m rt)

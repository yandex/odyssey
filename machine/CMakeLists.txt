
set(machine_library machine)

set(machine_src mm.c mm_context.c mm_fiber.c mm_scheduler.c mm_io.c mm_connect.c mm_dns.c)

add_library(machine_library_shared SHARED ${machine_src})
set_target_properties(machine_library_shared PROPERTIES OUTPUT_NAME ${machine_library})
target_link_libraries(machine_library_shared ${machine_libraries} m rt)

if (BUILD_STATIC)
  add_library(machine_library_static STATIC ${machine_src})
  set_target_properties(machine_library_static PROPERTIES OUTPUT_NAME ${machine_library})
endif(BUILD_STATIC)

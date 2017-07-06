set(so_library shapito)
set(so_src md5.c password.c parameter.c read.c beread.c feread.c)

add_library(so_library_static STATIC ${so_src})
set_target_properties(so_library_static PROPERTIES OUTPUT_NAME ${so_library})

if (BUILD_SHARED)
  add_library(so_library_shared SHARED ${so_src})
  set_target_properties(so_library_shared PROPERTIES OUTPUT_NAME ${so_library})
endif(BUILD_SHARED)

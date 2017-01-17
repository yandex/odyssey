set(so_library soprano)
set(so_src so_read.c so_beread.c so_feread.c)

add_library(so_library_shared SHARED ${so_src})
set_target_properties(so_library_shared PROPERTIES OUTPUT_NAME ${so_library})

if (BUILD_STATIC)
  add_library(so_library_static STATIC ${so_src})
  set_target_properties(so_library_static PROPERTIES OUTPUT_NAME ${so_library})
endif(BUILD_STATIC)

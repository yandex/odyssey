set(so_library "soprano")
set(so_src so_read.c so_beread.c so_feread.c)

add_library(${so_library} SHARED ${so_src})

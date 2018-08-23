set(kw_library kiwi)
#set(kiwi_src msg.c md5.c password.c parameter.c read.c be_read.c fe_read.c)
set(kw_src dummy.c)

add_library(kw_library_static STATIC ${kw_src})
set_target_properties(kw_library_static PROPERTIES OUTPUT_NAME ${kw_library})

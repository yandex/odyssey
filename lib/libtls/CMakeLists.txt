set(tls_library tls)
set(tls_src
    tls.c
    tls_client.c
    tls_config.c
    tls_conninfo.c
    tls_server.c
    tls_ocsp.c
    tls_peer.c
    tls_util.c
    tls_verify.c
    tls_compat.c)
add_library(tls_library_static STATIC ${tls_src})
set_target_properties(tls_library_static PROPERTIES OUTPUT_NAME ${tls_library})

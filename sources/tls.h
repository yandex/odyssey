#ifndef OD_TLS_H
#define OD_TLS_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

machine_tls_t*
od_tls_frontend(od_configlisten_t*);

int
od_tls_frontend_accept(od_client_t*, od_logger_t*, od_configlisten_t*,
                       machine_tls_t*);

machine_tls_t*
od_tls_backend(od_configstorage_t*);

int
od_tls_backend_connect(od_server_t*, od_logger_t*, shapito_stream_t*,
                       od_configstorage_t*);

#endif /* OD_TLS_H */

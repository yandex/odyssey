#ifndef OD_TLS_H
#define OD_TLS_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

machine_tls_t*
od_tls_frontend(od_scheme_t*);

int
od_tls_frontend_accept(od_client_t*, od_log_t*, od_scheme_t*,
                       machine_tls_t*);

machine_tls_t*
od_tls_backend(od_schemestorage_t*);

int
od_tls_backend_connect(od_server_t*, od_log_t*, od_schemestorage_t*);

#endif /* OD_TLS_H */

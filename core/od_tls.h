#ifndef OD_TLS_H_
#define OD_TLS_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

machine_tls_t
od_tls_client(od_pooler_t*, od_scheme_t*);

machine_tls_t
od_tls_server(od_pooler_t*, od_schemeserver_t*);

#endif

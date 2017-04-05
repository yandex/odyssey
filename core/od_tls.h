#ifndef OD_TLS_H_
#define OD_TLS_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

machine_tls_t
od_tlsfe(machine_t, od_scheme_t*);

int
od_tlsfe_accept(machine_t, machine_io_t, machine_tls_t,
                so_stream_t*,
                od_log_t*,
                char*,
                od_scheme_t*,
                so_bestartup_t*);

machine_tls_t
od_tlsbe(machine_t, od_schemeserver_t*);

int
od_tlsbe_connect(machine_t, machine_io_t, machine_tls_t,
                 so_stream_t*,
                 od_log_t*,
                 char*,
                 od_schemeserver_t*);

#endif

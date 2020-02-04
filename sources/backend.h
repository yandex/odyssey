#ifndef ODYSSEY_BACKEND_H
#define ODYSSEY_BACKEND_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef int (*od_backend_query_response_cb_t)(uint32_t, uint32_t, void*, uint32_t, void**);

int  od_backend_connect(od_server_t*, char*, kiwi_params_t*);
int  od_backend_connect_cancel(od_server_t*, od_rule_storage_t*, kiwi_key_t*);
int  od_backend_connect_query(od_server_t *, char*, char *, int);
void od_backend_close_connection(od_server_t*);
void od_backend_close(od_server_t*);
void od_backend_error(od_server_t*, char*, char*, uint32_t);
int  od_backend_update_parameter(od_server_t*, char*, char*, uint32_t, int);
int  od_backend_ready(od_server_t*, char*, uint32_t);
int  od_backend_ready_wait(od_server_t*, char*, int, uint32_t);
int  od_backend_query(od_server_t*, char*, char*, int);
int  od_backend_read_query_response(od_server_t*, char*,
							        od_backend_query_response_cb_t, void**);

#endif /* ODYSSEY_BACKEND_H */

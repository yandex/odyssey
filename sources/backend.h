#ifndef OD_BACKEND_H
#define OD_BACKEND_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int  od_backend_connect(od_server_t*, shapito_stream_t*, char*);
int  od_backend_connect_cancel(od_server_t*, shapito_stream_t*, od_configstorage_t*,
                               shapito_key_t*);
void od_backend_close_connection(od_server_t*);
void od_backend_close(od_server_t*);
void od_backend_error(od_server_t*, char*, char*, int);
int  od_backend_ready(od_server_t*, char*, char*, int);
int  od_backend_ready_wait(od_server_t*, shapito_stream_t*, char*, int, uint32_t);
int  od_backend_query(od_server_t*, shapito_stream_t*, char*, char*, int);
int  od_backend_deploy(od_server_t*, char*, char*, int);
int  od_backend_deploy_wait(od_server_t*, shapito_stream_t*, char*, uint32_t);

#endif /* OD_BACKEND_H */

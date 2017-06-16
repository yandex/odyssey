#ifndef OD_BACKEND_H
#define OD_BACKEND_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

int  od_backend_connect(od_server_t*);
int  od_backend_connect_cancel(od_server_t*, od_schemeserver_t*, so_key_t*);
void od_backend_close(od_server_t*);
int  od_backend_terminate(od_server_t*);
int  od_backend_reset(od_server_t*);
void od_backend_error(od_server_t*, char*, uint8_t*, int);
int  od_backend_ready(od_server_t*, uint8_t*, int);
int  od_backend_configure(od_server_t*, so_bestartup_t*);
int  od_backend_discard(od_server_t*);

#endif /* OD_BACKEND_H */

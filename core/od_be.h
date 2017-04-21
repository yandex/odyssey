#ifndef OD_BE_H_
#define OD_BE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_beterminate(od_server_t*);
int od_beclose(od_server_t*);
int od_berelease(od_server_t*);
int od_beset_ready(od_server_t*, uint8_t*, int);
int od_beconfigure(od_server_t*, so_bestartup_t*);

od_server_t*
od_bepop(od_pooler_t*, od_route_t*, od_client_t*);

#endif

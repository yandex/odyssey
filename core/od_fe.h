#ifndef OD_FE_H_
#define OD_FE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int  od_festartup(odclient_t*);
int  od_feauth(odclient_t*);
int  od_feready(odclient_t*);
int  od_feerror(odclient_t*, char*, ...);
void od_feclose(odclient_t*);

#endif

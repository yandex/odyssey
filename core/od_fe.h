#ifndef OD_FE_H_
#define OD_FE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int  od_festartup(od_client_t*);
int  od_fekey(od_client_t*);
int  od_fesetup(od_client_t*);
int  od_feready(od_client_t*);
int  od_feerror(od_client_t*, char*, ...);
void od_feclose(od_client_t*);

#endif

#ifndef OD_BE_H_
#define OD_BE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_beterminate(odserver_t*);
int od_beclose(odserver_t*);
int od_bereset(odserver_t*);
int od_beset_not_ready(odserver_t*);
int od_beset_ready(odserver_t*, sostream_t*);

odserver_t*
od_bepop(odpooler_t*, odroute_t*);

#endif

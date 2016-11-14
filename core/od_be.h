#ifndef OD_BE_H_
#define OD_BE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_beclose(odpooler_t*, odserver_t*);

odserver_t*
od_bepop(odpooler_t*, odroute_t*);

#endif

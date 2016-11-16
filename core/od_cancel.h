#ifndef OD_CANCEL_H_
#define OD_CANCEL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_cancel(odpooler_t*, sokey_t*);
int od_cancel_of(odpooler_t*, odscheme_server_t*, sokey_t*);

#endif

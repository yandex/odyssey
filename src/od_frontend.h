#ifndef OD_FRONTEND_H
#define OD_FRONTEND_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int  od_frontend_error(od_client_t*, char*, char*, ...);
void od_frontend(void*);

#endif /* OD_FRONTEND_H */

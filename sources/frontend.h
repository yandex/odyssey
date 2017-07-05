#ifndef OD_FRONTEND_H
#define OD_FRONTEND_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int  od_frontend_error(od_client_t*, char*, char*, ...);
void od_frontend(void*);

#endif /* OD_FRONTEND_H */

#ifndef OD_DEPLOY_H
#define OD_DEPLOY_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_deploy(od_server_t*, char*, shapito_parameters_t*, int);

int od_deploy_write(od_server_t*, char*, shapito_stream_t*, shapito_parameters_t*);

#endif /* OD_DEPLOY_H */

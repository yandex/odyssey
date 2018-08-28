#ifndef ODYSSEY_CANCEL_H
#define ODYSSEY_CANCEL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

int od_cancel(od_global_t*, od_config_storage_t*, kiwi_key_t*, od_id_t*);
int od_cancel_find(od_route_pool_t*, kiwi_key_t*, od_router_cancel_t*);

#endif /* ODYSSEY_CANCEL_H */

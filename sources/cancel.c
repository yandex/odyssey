
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <kiwi/kiwi.h>

#include <types.h>
#include <storage.h>
#include <id.h>
#include <global.h>
#include <instance.h>
#include <server.h>
#include <backend.h>

int od_cancel(od_global_t *global, od_rule_storage_t *storage,
	      const od_address_t *address, kiwi_key_t *key, od_id_t *server_id)
{
	od_instance_t *instance = global->instance;
	od_log(&instance->logger, "cancel", NULL, NULL, "cancel for %s%.*s",
	       server_id->id_prefix, sizeof(server_id->id), server_id->id);
	od_server_t *server = od_server_allocate(0);
	server->global = global;
	od_backend_connect_cancel(server, storage, address, key);
	od_backend_close_connection(server);
	od_backend_close(server);
	return 0;
}

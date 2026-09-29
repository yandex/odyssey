/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <console_databases.h>
#include <od_memory.h>
#include <rules.h>

typedef struct {
	const char *name;
	const char *database;
	const char *force_user;
	od_address_range_t address_range;
	od_rule_conn_type_t conn_type;
	od_rule_routing_type_t routing;
	int db_is_default;
	int user_is_default;
} od_console_database_key_t;

static int od_console_database_key_compare(const void *a, const void *b)
{
	const od_console_database_key_t *ka = a;
	const od_console_database_key_t *kb = b;

	return strcmp(ka->name, kb->name) != 0 ||
	       strcmp(ka->database, kb->database) != 0 ||
	       strcmp(ka->force_user, kb->force_user) != 0 ||
	       ka->conn_type != kb->conn_type || ka->routing != kb->routing ||
	       ka->db_is_default != kb->db_is_default ||
	       ka->user_is_default != kb->user_is_default ||
	       ka->address_range.is_default != kb->address_range.is_default ||
	       !od_address_range_equals(&ka->address_range, &kb->address_range);
}

static mm_hash_t od_console_database_key_hash(const void *ptr)
{
	const od_console_database_key_t *key = ptr;
	mm_hash_t hash = mm_xxh64_hash(key->name, strlen(key->name), 0);
	hash = mm_xxh64_hash(key->database, strlen(key->database), hash);
	return mm_xxh64_hash(key->force_user, strlen(key->force_user), hash);
}

static void od_console_database_key_free(void *ptr)
{
	od_console_database_key_t *key = ptr;
	od_address_range_destroy(&key->address_range);
}

int od_console_database_rows_init(od_console_database_rows_t *rows,
				  size_t route_count)
{
	memset(rows, 0, sizeof(*rows));
	rows->index = mm_hashmap_create(
		route_count, 0, sizeof(od_console_database_key_t), sizeof(int),
		od_console_database_key_compare, od_console_database_key_hash,
		od_console_database_key_free, NULL, NULL);
	if (rows->index == NULL) {
		return NOT_OK_RESPONSE;
	}
	if (route_count > 0) {
		rows->items = od_malloc(route_count * sizeof(*rows->items));
		if (rows->items == NULL) {
			od_console_database_rows_free(rows);
			return NOT_OK_RESPONSE;
		}
	}
	return OK_RESPONSE;
}

static void od_console_database_row_free(od_console_database_row_t *row)
{
	od_free(row->name);
	od_free(row->host);
	od_free(row->database);
	od_free(row->force_user);
}

void od_console_database_rows_free(od_console_database_rows_t *rows)
{
	mm_hashmap_free(rows->index);
	for (int i = 0; i < rows->count; i++) {
		od_console_database_row_free(&rows->items[i]);
	}
	od_free(rows->items);
	memset(rows, 0, sizeof(*rows));
}

int od_console_database_rows_add(od_console_database_rows_t *rows,
				 const od_rule_t *rule, const char *name,
				 int name_len, int current_connections)
{
	/* Keep independent rules separate, but merge their reload generations. */
	od_console_database_key_t key = {
		.name = name,
		.database = rule->db_name,
		.force_user = rule->user_name,
		.address_range = rule->address_range,
		.conn_type = rule->conn_type,
		.routing = rule->pool->routing,
		.db_is_default = rule->db_is_default,
		.user_is_default = rule->user_is_default,
	};
	const char *host = rule->storage->host;
	if (host == NULL) {
		host = "";
	}

	mm_hashmap_keylock_t lock;
	int rc = mm_hashmap_lock_key(rows->index, &lock, &key, 0);
	if (rc != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}
	if (lock.found) {
		int *index = mm_hashmap_kvp_val(rows->index, lock.kvp);
		od_console_database_row_t *row = &rows->items[*index];
		if (row->representative_obsolete && !rule->obsolete) {
			char *new_host = od_strdup(host);
			if (new_host == NULL) {
				mm_hashmap_unlock_key(rows->index, &lock);
				return NOT_OK_RESPONSE;
			}
			od_free(row->host);
			row->host = new_host;
			row->host_len = strlen(host);
			row->port = rule->storage->port;
			row->pool_size = rule->pool->size;
			row->client_max = rule->client_max;
			row->pool_type = rule->pool->pool_type;
			row->representative_obsolete = 0;
		}
		row->current_connections += current_connections;
		mm_hashmap_unlock_key(rows->index, &lock);
		return OK_RESPONSE;
	}

	od_console_database_row_t *row = &rows->items[rows->count];
	*row = (od_console_database_row_t){
		.name = od_strndup(name, name_len),
		.name_len = name_len,
		.host = od_strdup(host),
		.host_len = strlen(host),
		.port = rule->storage->port,
		.database = od_strndup(rule->db_name, rule->db_name_len),
		.database_len = rule->db_name_len,
		.force_user = od_strndup(rule->user_name, rule->user_name_len),
		.force_user_len = rule->user_name_len,
		.pool_size = rule->pool->size,
		.client_max = rule->client_max,
		.current_connections = current_connections,
		.pool_type = rule->pool->pool_type,
		.representative_obsolete = rule->obsolete,
	};
	if (row->name == NULL || row->host == NULL || row->database == NULL ||
	    row->force_user == NULL) {
		goto error;
	}

	/* The index borrows strings owned by rows. */
	key.name = row->name;
	key.database = row->database;
	key.force_user = row->force_user;
	key.address_range.string_value = NULL;
	if (od_address_range_copy(&rule->address_range, &key.address_range)) {
		goto error;
	}
	rc = mm_hashmap_lock_key(rows->index, &lock, &key, MM_HASHMAP_CREATE);
	if (rc != OK_RESPONSE) {
		od_address_range_destroy(&key.address_range);
		goto error;
	}
	od_assert(!lock.found);
	int *index = mm_hashmap_kvp_val(rows->index, lock.kvp);
	*index = rows->count++;
	mm_hashmap_unlock_key(rows->index, &lock);
	return OK_RESPONSE;

error:
	od_console_database_row_free(row);
	return NOT_OK_RESPONSE;
}

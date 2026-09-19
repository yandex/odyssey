#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <od_c.h>
#include <types.h>
#include <pool.h>
#include <machinarium/ds/hm.h>

typedef struct {
	char *name;
	int name_len;
	char *host;
	int host_len;
	int port;
	char *database;
	int database_len;
	char *force_user;
	int force_user_len;
	int pool_size;
	int client_max;
	int current_connections;
	od_rule_pool_type_t pool_type;
	int representative_obsolete;
} od_console_database_row_t;

typedef struct {
	od_console_database_row_t *items;
	int count;
	mm_hashmap_t *index;
} od_console_database_rows_t;

int od_console_database_rows_init(od_console_database_rows_t *rows,
				  size_t route_count);
void od_console_database_rows_free(od_console_database_rows_t *rows);
int od_console_database_rows_add(od_console_database_rows_t *rows,
				 const od_rule_t *rule, const char *name,
				 int name_len, int current_connections);

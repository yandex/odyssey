#include <odyssey.h>

#include <console_databases.h>
#include <rules.h>

#include <tests/odyssey_test.h>

static int add_row(od_console_database_rows_t *rows, const char *name,
		   const char *host, int port, const char *database,
		   const char *force_user, int pool_size, int client_max,
		   int current_connections, od_rule_pool_type_t pool_type,
		   int obsolete)
{
	od_rule_pool_t pool = { .size = pool_size, .pool_type = pool_type };
	od_rule_storage_t storage = { .host = (char *)host, .port = port };
	od_rule_t rule = {
		.db_name = (char *)database,
		.db_name_len = strlen(database),
		.user_name = (char *)force_user,
		.user_name_len = strlen(force_user),
		.address_range = { .string_value = "all",
				   .string_value_len = 3,
				   .is_default = 1 },
		.pool = &pool,
		.storage = &storage,
		.client_max = client_max,
		.obsolete = obsolete,
	};
	return od_console_database_rows_add(rows, &rule, name, strlen(name),
					    current_connections);
}

static void test_merge_same_force_user(void)
{
	od_console_database_rows_t rows;
	test(od_console_database_rows_init(&rows, 2) == OK_RESPONSE);

	test(add_row(&rows, "mydatabase", "10.3.11.11", 5432, "mydatabase",
		     "kafka_connect", 500, 0, 0, OD_RULE_POOL_SESSION,
		     0) == OK_RESPONSE);
	test(add_row(&rows, "mydatabase", "10.3.11.11", 5432, "mydatabase",
		     "kafka_connect", 500, 0, 1, OD_RULE_POOL_SESSION,
		     0) == OK_RESPONSE);

	test(rows.count == 1);
	test(rows.items[0].current_connections == 1);
	test(rows.items[0].pool_size == 500);
	test(strcmp(rows.items[0].force_user, "kafka_connect") == 0);

	od_console_database_rows_free(&rows);
}

static void test_keep_distinct_force_user(void)
{
	od_console_database_rows_t rows;
	test(od_console_database_rows_init(&rows, 2) == OK_RESPONSE);

	test(add_row(&rows, "mydatabase", "10.3.11.11", 5432, "mydatabase",
		     "mydatabase", 500, 0, 95, OD_RULE_POOL_SESSION,
		     0) == OK_RESPONSE);
	test(add_row(&rows, "mydatabase", "10.3.11.11", 5432, "mydatabase",
		     "kafka_connect", 500, 0, 1, OD_RULE_POOL_SESSION,
		     0) == OK_RESPONSE);

	test(rows.count == 2);
	test(rows.items[0].current_connections == 95);
	test(strcmp(rows.items[0].force_user, "mydatabase") == 0);
	test(rows.items[1].current_connections == 1);
	test(strcmp(rows.items[1].force_user, "kafka_connect") == 0);

	od_console_database_rows_free(&rows);
}

static void test_current_metadata_does_not_depend_on_order(void)
{
	od_rule_pool_t pools[] = {
		{ .size = 10, .pool_type = OD_RULE_POOL_SESSION },
		{ .size = 20, .pool_type = OD_RULE_POOL_TRANSACTION },
	};
	od_rule_storage_t storages[] = {
		{ .host = "127.0.0.1", .port = 5432 },
		{ .host = "localhost", .port = 5433 },
	};
	od_rule_t obsolete = {
		.db_name = "postgres",
		.db_name_len = 8,
		.user_name = "postgres",
		.user_name_len = 8,
		.address_range = { .string_value = "all",
				   .string_value_len = 3,
				   .is_default = 1 },
		.pool = &pools[0],
		.storage = &storages[0],
		.obsolete = 1,
	};
	od_rule_t current = obsolete;
	current.pool = &pools[1];
	current.storage = &storages[1];
	current.client_max = 7;
	current.obsolete = 0;

	const od_rule_t *rules[] = { &obsolete, &current };
	int connections[] = { 3, 1 };
	for (int order = 0; order < 2; order++) {
		od_console_database_rows_t rows;
		test(od_console_database_rows_init(&rows, 2) == OK_RESPONSE);
		for (int i = 0; i < 2; i++) {
			int index = (i + order) % 2;
			test(od_console_database_rows_add(
				     &rows, rules[index], "postgres", 8,
				     connections[index]) == OK_RESPONSE);
		}

		test(rows.count == 1);
		test(rows.items[0].current_connections == 4);
		test(strcmp(rows.items[0].host, "localhost") == 0);
		test(rows.items[0].port == 5433);
		test(strcmp(rows.items[0].database, "postgres") == 0);
		test(rows.items[0].pool_size == 20);
		test(rows.items[0].client_max == 7);
		test(rows.items[0].pool_type == OD_RULE_POOL_TRANSACTION);
		test(rows.items[0].representative_obsolete == 0);

		od_console_database_rows_free(&rows);
	}
}

static void test_obsolete_only_keeps_row(void)
{
	od_console_database_rows_t rows;
	test(od_console_database_rows_init(&rows, 2) == OK_RESPONSE);

	test(add_row(&rows, "postgres", "127.0.0.1", 5432, "postgres",
		     "postgres", 10, 0, 1, OD_RULE_POOL_SESSION,
		     1) == OK_RESPONSE);
	test(add_row(&rows, "postgres", "localhost", 5432, "postgres",
		     "postgres", 20, 5, 2, OD_RULE_POOL_TRANSACTION,
		     1) == OK_RESPONSE);

	test(rows.count == 1);
	test(rows.items[0].current_connections == 3);
	test(strcmp(rows.items[0].host, "127.0.0.1") == 0);
	test(rows.items[0].pool_size == 10);
	test(rows.items[0].representative_obsolete == 1);

	od_console_database_rows_free(&rows);
}

static void test_keep_distinct_database_rules(void)
{
	od_console_database_rows_t rows;
	test(od_console_database_rows_init(&rows, 2) == OK_RESPONSE);

	test(add_row(&rows, "postgres", "127.0.0.1", 5432, "alpha", "postgres",
		     10, 0, 1, OD_RULE_POOL_SESSION, 0) == OK_RESPONSE);
	test(add_row(&rows, "postgres", "127.0.0.1", 5432, "beta", "postgres",
		     20, 0, 2, OD_RULE_POOL_TRANSACTION, 0) == OK_RESPONSE);

	test(rows.count == 2);
	test(strcmp(rows.items[0].database, "alpha") == 0);
	test(rows.items[0].pool_size == 10);
	test(rows.items[0].pool_type == OD_RULE_POOL_SESSION);
	test(rows.items[0].current_connections == 1);
	test(strcmp(rows.items[1].database, "beta") == 0);
	test(rows.items[1].pool_size == 20);
	test(rows.items[1].pool_type == OD_RULE_POOL_TRANSACTION);
	test(rows.items[1].current_connections == 2);

	od_console_database_rows_free(&rows);
}

static void test_keep_distinct_rule_selectors(void)
{
	od_console_database_rows_t rows;
	test(od_console_database_rows_init(&rows, 6) == OK_RESPONSE);
	od_rule_pool_t pool = { .routing = OD_RULE_POOL_CLIENT_VISIBLE };
	od_rule_storage_t storage = { 0 };
	od_rule_t base = {
		.db_name = "postgres",
		.db_name_len = 8,
		.user_name = "postgres",
		.user_name_len = 8,
		.address_range = { .string_value = "all",
				   .string_value_len = 3,
				   .is_default = 1 },
		.pool = &pool,
		.storage = &storage,
	};

	for (int i = 0; i < 6; i++) {
		od_rule_t rule = base;
		switch (i) {
		case 1:
			rule.conn_type = OD_RULE_CONN_TYPE_HOST;
			break;
		case 2:
			rule.address_range = (od_address_range_t){
				.string_value = "localhost",
				.string_value_len = 9,
				.is_hostname = 1,
			};
			break;
		case 3:
			rule.db_is_default = 1;
			break;
		case 4:
			rule.user_is_default = 1;
			break;
		case 5:
			pool.routing = OD_RULE_POOL_INTERNAL;
			break;
		default:
			break;
		}
		test(od_console_database_rows_add(&rows, &rule, "postgres", 8,
						  1) == OK_RESPONSE);
		test(rows.count == i + 1);
	}
	for (int i = 0; i < rows.count; i++) {
		test(rows.items[i].current_connections == 1);
	}
	od_console_database_rows_free(&rows);
}

void odyssey_test_console_databases(void)
{
	test_merge_same_force_user();
	test_keep_distinct_force_user();
	test_current_metadata_does_not_depend_on_order();
	test_obsolete_only_keeps_row();
	test_keep_distinct_database_rules();
	test_keep_distinct_rule_selectors();
}

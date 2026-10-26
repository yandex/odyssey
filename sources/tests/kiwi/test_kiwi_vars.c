#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

#include <limits.h>

static void check_search_path(const char *value, int smart,
			      const char *expected)
{
	kiwi_vars_t client, server;
	kiwi_vars_init(&client);
	kiwi_vars_init(&server);
	test(kiwi_vars_set(&client, KIWI_VAR_SEARCH_PATH, value,
			   strlen(value) + 1) == 0);
	char *query;
	int normalize_search_path;
	int size = kiwi_vars_cas(&client, &server, &query, smart,
				 &normalize_search_path);
	test(size == (int)strlen(expected));
	test(strcmp(query, expected) == 0);
	free(query);
	kiwi_vars_free(&client);
	kiwi_vars_free(&server);
}

void kiwi_test_vars_cas(void)
{
	/* Only restoration that preserves the name list may cache normalization. */
	for (int smart = 0; smart <= 1; smart++) {
		const char *paths[] = { "a,b,c", "\"my-schema\", public" };
		for (int i = 0; i < 2; i++) {
			kiwi_vars_t client, server;
			kiwi_vars_init(&client);
			kiwi_vars_init(&server);
			test(kiwi_vars_set(&client, KIWI_VAR_SEARCH_PATH,
					   paths[i],
					   strlen(paths[i]) + 1) == 0);
			char *query;
			int normalize_search_path = -1;
			test(kiwi_vars_cas(&client, &server, &query, smart,
					   &normalize_search_path) > 0);
			test(normalize_search_path == (i == 0 || smart));
			free(query);
			kiwi_vars_free(&client);
			kiwi_vars_free(&server);
		}
	}

	for (int smart = 0; smart <= 1; smart++) {
		check_search_path("a,b,c", smart, "SET search_path=a,b,c;");
		check_search_path(
			"\"my schema\", \"a\"\"b\", public", smart,
			"SET search_path=\"my schema\", \"a\"\"b\", public;");
		check_search_path("\"$user\", public", smart,
				  "SET search_path=\"$user\", public;");
		check_search_path("\"$$\", \"$a$\", public", smart,
				  "SET search_path=\"$$\", \"$a$\", public;");
		check_search_path(
			"PUBLIC, \"MixedCase\", public", smart,
			"SET search_path=PUBLIC, \"MixedCase\", public;");
		check_search_path("users, \"user\", public", smart,
				  "SET search_path=users, \"user\", public;");
	}
	/* Complex values keep the legacy escaped SET when smart mode is off. */
	check_search_path("\"my-schema\", public", 0,
			  "SET search_path=E'\"my-schema\", public';");
	check_search_path(
		"\"O'Reilly\", \"a\\b\", public", 0,
		"SET search_path=E'\"O''Reilly\", \"a\\\\b\", public';");
	check_search_path("", 0, "SET search_path=E'';");
	check_search_path("public$; SELECT 1 AS x$", 0,
			  "SET search_path=E'public$; SELECT 1 AS x$';");
	check_search_path(
		"public;CREATE/**/TABLE/**/ody_inj();--", 0,
		"SET search_path=E'public;CREATE/**/TABLE/**/ody_inj();--';");
	check_search_path("default", 0, "SET search_path=E'default';");
	check_search_path("UsEr", 0, "SET search_path=E'UsEr';");
	check_search_path("current_user", 0,
			  "SET search_path=E'current_user';");
	check_search_path("$user", 0, "SET search_path=E'$user';");
	check_search_path("1abc", 0, "SET search_path=E'1abc';");
	check_search_path("ALL, public", 0, "SET search_path=E'ALL, public';");
	check_search_path(
		"UsEr", 1,
		"SELECT pg_catalog.set_config('search_path',E'UsEr',false);");
	check_search_path(
		"current_user", 1,
		"SELECT pg_catalog.set_config('search_path',E'current_user',false);");
	check_search_path(
		"$user", 1,
		"SELECT pg_catalog.set_config('search_path',E'$user',false);");
	check_search_path(
		"1abc", 1,
		"SELECT pg_catalog.set_config('search_path',E'1abc',false);");
	check_search_path(
		"ALL, public", 1,
		"SELECT pg_catalog.set_config('search_path',E'ALL, public',false);");
	check_search_path(
		"\"my-schema\", public", 1,
		"SELECT pg_catalog.set_config('search_path',E'\"my-schema\", public',false);");
	check_search_path(
		"\"O'Reilly\", \"a\\b\", public", 1,
		"SELECT pg_catalog.set_config('search_path',E'\"O''Reilly\", \"a\\\\b\", public',false);");
	check_search_path(
		"", 1,
		"SELECT pg_catalog.set_config('search_path',E'',false);");
	check_search_path(
		"public$; SELECT 1 AS x$", 1,
		"SELECT pg_catalog.set_config('search_path',E'public$; SELECT 1 AS x$',false);");
	check_search_path(
		"public;CREATE/**/TABLE/**/ody_inj();--", 1,
		"SELECT pg_catalog.set_config('search_path',E'public;CREATE/**/TABLE/**/ody_inj();--',false);");
	check_search_path(
		"default", 1,
		"SELECT pg_catalog.set_config('search_path',E'default',false);");

	char identifier[65];
	char expected_identifier[160];
	memset(identifier, 'a', 63);
	identifier[63] = 0;
	snprintf(expected_identifier, sizeof(expected_identifier),
		 "SET search_path=%s;", identifier);
	check_search_path(identifier, 1, expected_identifier);
	check_search_path(identifier, 0, expected_identifier);
	identifier[63] = 'a';
	identifier[64] = 0;
	snprintf(expected_identifier, sizeof(expected_identifier),
		 "SELECT pg_catalog.set_config('search_path',E'%s',false);",
		 identifier);
	check_search_path(identifier, 1, expected_identifier);
	snprintf(expected_identifier, sizeof(expected_identifier),
		 "SET search_path=E'%s';", identifier);
	check_search_path(identifier, 0, expected_identifier);

	char path[2048] = "public";
	for (int i = 0; i < 200; i++) {
		strcat(path, ", public");
	}
	char long_query[2200];
	snprintf(long_query, sizeof(long_query), "SET search_path=%s;", path);
	check_search_path(path, 1, long_query);
	check_search_path(path, 0, long_query);

	kiwi_vars_t client, server;
	kiwi_vars_init(&client);
	kiwi_vars_init(&server);
	char *query;
	int normalize_search_path = -1;
	test(kiwi_vars_cas(&client, &server, &query, 1,
			   &normalize_search_path) == 0);
	test(normalize_search_path == 0);
	test(query == NULL);
	test(kiwi_vars_set(&client, KIWI_VAR_COMPRESSION, "on", sizeof("on")) ==
	     0);
	test(kiwi_vars_set(&client, KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			   "read-write", sizeof("read-write")) == 0);
	test(kiwi_vars_cas(&client, &server, &query, 1,
			   &normalize_search_path) == 0);
	test(query == NULL);
	test(kiwi_vars_set(&client, KIWI_VAR_SEARCH_PATH,
			   "\"my-schema\", public",
			   sizeof("\"my-schema\", public")) == 0);
	test(kiwi_vars_set(&client, KIWI_VAR_TRANSACTION_ISOLATION,
			   "serializable", sizeof("serializable")) == 0);
	const char *expected =
		"SET transaction_isolation=E'serializable';"
		"SELECT pg_catalog.set_config('search_path',E'\"my-schema\", public',false);";
	test(kiwi_vars_cas(&client, &server, &query, 1,
			   &normalize_search_path) == (int)strlen(expected));
	test(normalize_search_path == 1);
	test(strcmp(query, expected) == 0);
	free(query);
	test(kiwi_vars_override(&server, &client) == 0);
	test(kiwi_vars_cas(&client, &server, &query, 1,
			   &normalize_search_path) == 0);
	test(normalize_search_path == 0);
	test(query == NULL);
	test(kiwi_vars_set(&client, KIWI_VAR_APPLICATION_NAME, "test",
			   sizeof("test")) == 0);
	kiwi_var_t *var = kiwi_vars_of(&client, KIWI_VAR_WORK_MEM);
	test(kiwi_var_set(var, KIWI_VAR_WORK_MEM, "1MB", sizeof("1MB")) == 0);
	var->value_len = INT_MAX;
	normalize_search_path = -1;
	test(kiwi_vars_cas(&client, &server, &query, 1,
			   &normalize_search_path) == -1);
	test(normalize_search_path == 0);
	test(query == NULL);
	kiwi_vars_free(&client);
	kiwi_vars_free(&server);
}

#include <odyssey.h>
#include <tests/odyssey_test.h>

#include <instance.h>
#include <cfg_import.h>

#define QUERY "SELECT usename, passwd FROM pg_shadow WHERE usename=$1"
#define INTERNAL_BODY                                                       \
	"authentication \"none\" storage \"backend\" pool \"transaction\" " \
	"pool_routing \"internal\""
#define INTERNAL \
	"database \"postgres\" { user \"authq\" { " INTERNAL_BODY " } }\n"
#define BIND_ERROR "auth_query must use $1 as its only bind parameter"
#define ROUTE_ERROR "auth_query has no remote internal route"

static void check_config(const char *query, const char *routes,
			 const char *expected_error)
{
	char path[] = "/tmp/odyssey-auth-query-config-XXXXXX";
	int fd = mkstemp(path);
	test(fd != -1);
	FILE *config = fdopen(fd, "w");
	test(config != NULL);
	fputs("workers 1\n"
	      "unix_socket_dir \"/tmp\"\n"
	      "unix_socket_mode \"0700\"\n"
	      "log_format \"%m\\n\"\n"
	      "listen { host \"127.0.0.1\" port 8550 }\n"
	      "storage \"backend\" { type \"remote\" host \"127.0.0.1\" port 8562 }\n"
	      "database \"qa_auth\" { user default {\n"
	      "authentication \"scram-sha-256\" storage \"backend\" pool \"transaction\"\n"
	      "auth_query_db \"postgres\" auth_query_user \"authq\"\n"
	      "auth_query \"",
	      config);
	for (const char *p = query; *p; p++) {
		if (*p == '"') {
			fputc('\\', config);
		}
		fputc(*p, config);
	}
	fprintf(config, "\"\n} }\n%s", routes);
	test(fclose(config) == 0);

	char log_path[] = "/tmp/odyssey-auth-query-log-XXXXXX";
	int log_fd = mkstemp(log_path);
	test(log_fd != -1);
	od_logger_t logger = { 0 };
	test(od_logger_init(&logger, NULL) == OK_RESPONSE);
	logger.log_stdout = 0;
	od_logger_set_format(&logger, "%m\n");
	atomic_store(&logger.fd, log_fd);

	int rc = od_cfg_check(&logger, path, NULL);
	test(lseek(log_fd, 0, SEEK_SET) == 0);
	char output[8192];
	ssize_t size = read(log_fd, output, sizeof(output) - 1);
	test(size >= 0);
	output[size] = '\0';
	if ((expected_error == NULL && rc != 0) ||
	    (expected_error != NULL &&
	     (rc == 0 || strstr(output, expected_error) == NULL))) {
		fprintf(stderr, "auth_query=[%s], routes=[%s], rc=%d\n%s\n",
			query, routes, rc, output);
		abort();
	}
	od_logger_close(&logger);
	test(unlink(path) == 0);
	test(unlink(log_path) == 0);
}

void odyssey_test_auth_query_config(void)
{
	static const char *valid[] = {
		QUERY,
		"SELECT $1, $1",
		"SELECT $01, 'password'",
		"SELECT $1, '$2' /* $3 /* $4 */ */ -- $5",
		"SELECT $1, $$ $2 $$",
		"SELECT $1, $quoted$ $2 $quoted$",
		"SELECT $1, \"$2\"",
		"SELECT $1, user$2 FROM pg_shadow",
		"SELECT $1, E'can\\'t use $2'",
	};
	for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
		check_config(valid[i], INTERNAL, NULL);
	}
	static const char *invalid[] = {
		"",
		"SELECT usename, passwd FROM pg_shadow WHERE usename='%u'",
		"SELECT '$1', 'password'",
		"SELECT \"$1\", passwd FROM pg_shadow",
		"SELECT $$ $1 $$, 'password'",
		"SELECT $quoted$ $1 $quoted$, 'password'",
		"SELECT user$1, passwd FROM pg_shadow",
		"SELECT usename, passwd FROM pg_shadow -- $1",
		"SELECT usename, passwd FROM pg_shadow /* $1 /* $1 */ */",
		"SELECT $2, 'password'",
		"SELECT $10, 'password'",
		"SELECT $1, $2",
		"SELECT $0, $1",
		"SELECT $2147483648, 'password'",
		"SELECT $1, 'unterminated",
		"SELECT $1 /* unterminated",
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		check_config(invalid[i], INTERNAL, BIND_ERROR);
	}

	check_config(QUERY, "", ROUTE_ERROR);
	check_config(
		QUERY,
		"database \"postgres\" { user \"authq\" { authentication \"none\" "
		"storage \"backend\" pool \"transaction\" } }",
		ROUTE_ERROR);
	check_config(QUERY,
		     "database \"other\" { user \"authq\" { " INTERNAL_BODY
		     " } }",
		     ROUTE_ERROR);
	check_config(QUERY,
		     "database \"postgres\" { user \"other\" { " INTERNAL_BODY
		     " } }",
		     ROUTE_ERROR);
	check_config(
		QUERY,
		"database \"postgres\" { user \"authq\" host { " INTERNAL_BODY
		" } }",
		ROUTE_ERROR);
	check_config(QUERY,
		     "database default { user \"authq\" { " INTERNAL_BODY
		     " } }",
		     NULL);
	check_config(QUERY,
		     "database \"postgres\" { user default { " INTERNAL_BODY
		     " } }",
		     NULL);
	check_config(QUERY,
		     "database default { user default { " INTERNAL_BODY " } }",
		     NULL);
	check_config(
		QUERY,
		"database default { user default { authentication \"none\" "
		"storage \"backend\" storage_password \"password\" pool \"transaction\" } }",
		NULL);
	check_config(
		QUERY,
		"database default { user default { authentication \"none\" "
		"storage \"backend\" pool \"transaction\" } }",
		ROUTE_ERROR);
	/* An internal rule for the client route cannot replace the auth route. */
	check_config(
		QUERY,
		"database \"qa_auth\" { user default { " INTERNAL_BODY " } } "
		"database default { user default { authentication \"none\" "
		"storage \"backend\" storage_password \"password\" pool \"transaction\" } }",
		NULL);
	check_config(
		QUERY,
		"storage \"console\" { type \"local\" } "
		"database \"postgres\" { user \"authq\" { authentication \"none\" "
		"storage \"console\" pool \"session\" pool_routing \"internal\" } }",
		ROUTE_ERROR);
	/* Use the candidate config's ordering, without any running instance. */
#define ORDERED_ROUTES                                                      \
	"database default { user default { " INTERNAL_BODY " } } "          \
	"storage \"console\" { type \"local\" } "                           \
	"database \"postgres\" { user \"authq\" { authentication \"none\" " \
	"storage \"console\" pool \"session\" pool_routing \"internal\" } } "
	check_config(QUERY, "sequential_routing yes\n" ORDERED_ROUTES, NULL);
	check_config(QUERY, "sequential_routing no\n" ORDERED_ROUTES,
		     ROUTE_ERROR);
	/* A group may contain authq after its membership is fetched. */
	check_config(
		QUERY,
		"database \"postgres\" { group \"auth_group\" { " INTERNAL_BODY
		" group_query \"SELECT rolname FROM pg_roles\" "
		"group_query_db \"postgres\" group_query_user \"monitor\" } "
		"user \"monitor\" { " INTERNAL_BODY " } }",
		NULL);
}

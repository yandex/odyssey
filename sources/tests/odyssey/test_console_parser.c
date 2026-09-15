#include <odyssey.h>
#include <tests/odyssey_test.h>

#include <console/ast.h>
#include <console/parser.h>
#include <alloc/linear.h>

#define PRINT_BUF_SIZE 1024
#define ARENA_SIZE 8192

static uint8_t s_arena_buf[ARENA_SIZE];
static od_linear_alloc_t s_arena;

static void on_error(const char *msg, void *userdata)
{
	strcpy((char *)userdata, msg);
}

static const char *parse_ok(const char *input)
{
	static char out[PRINT_BUF_SIZE];
	static char err[PRINT_BUF_SIZE];

	memset(err, 0, sizeof(err));
	memset(out, 0, sizeof(out));

	od_linear_alloc_reset(&s_arena);
	od_console_node_t *node =
		od_console_parse(input, strlen(input), &s_arena, on_error, err);

	if (node == NULL || strlen(err) != 0) {
		fprintf(stderr,
			"parse_ok failed: input=[%s] err=[%s] node=%p\n", input,
			err, (void *)node);
		abort();
	}

	int n = od_console_node_print(node, out, sizeof(out));
	test(n > 0 && (size_t)n < sizeof(out));
	return out;
}

static void parse_fail(const char *input)
{
	static char err[PRINT_BUF_SIZE];

	memset(err, 0, sizeof(err));
	od_linear_alloc_reset(&s_arena);
	od_console_node_t *node =
		od_console_parse(input, strlen(input), &s_arena, on_error, err);
	if (node != NULL) {
		fprintf(stderr,
			"parse_fail: expected failure but got node for [%s]\n",
			input);
		abort();
	}
	test(node == NULL);
}

/*
 * SHOW tests
 */
static void test_show_stats(void)
{
	test(strcmp(parse_ok("SHOW STATS"), "(show stats)") == 0);
}

static void test_show_help(void)
{
	test(strcmp(parse_ok("SHOW HELP"), "(show help)") == 0);
}

static void test_show_pools(void)
{
	test(strcmp(parse_ok("SHOW POOLS"), "(show pools)") == 0);
}

static void test_show_pools_extended(void)
{
	test(strcmp(parse_ok("SHOW POOLS_EXTENDED"), "(show pools_extended)") ==
	     0);
}

static void test_show_databases(void)
{
	test(strcmp(parse_ok("SHOW DATABASES"), "(show databases)") == 0);
}

static void test_show_servers(void)
{
	test(strcmp(parse_ok("SHOW SERVERS"), "(show servers)") == 0);
}

static void test_show_server_prep_stmts(void)
{
	test(strcmp(parse_ok("SHOW SERVER_PREP_STMTS"),
		    "(show server_prep_stmts)") == 0);
}

static void test_show_global_prepared_stmts(void)
{
	test(strcmp(parse_ok("SHOW GLOBAL_PREPARED_STATEMENTS"),
		    "(show global_prepared_statements)") == 0);
}

static void test_show_clients(void)
{
	test(strcmp(parse_ok("SHOW CLIENTS"), "(show clients)") == 0);
}

static void test_show_lists(void)
{
	test(strcmp(parse_ok("SHOW LISTS"), "(show lists)") == 0);
}

static void test_show_instance(void)
{
	test(strcmp(parse_ok("SHOW INSTANCE"), "(show instance)") == 0);
}

static void test_show_errors(void)
{
	test(strcmp(parse_ok("SHOW ERRORS"), "(show errors)") == 0);
}

static void test_show_errors_per_route(void)
{
	test(strcmp(parse_ok("SHOW ERRORS_PER_ROUTE"),
		    "(show errors_per_route)") == 0);
}

static void test_show_version(void)
{
	test(strcmp(parse_ok("SHOW VERSION"), "(show version)") == 0);
	test(strcmp(parse_ok("SHOW VERSION_EXTENDED"),
		    "(show version_extended)") == 0);
}

static void test_show_listen(void)
{
	test(strcmp(parse_ok("SHOW LISTEN"), "(show listen)") == 0);
}

static void test_show_storages(void)
{
	test(strcmp(parse_ok("SHOW STORAGES"), "(show storages)") == 0);
}

static void test_show_fds(void)
{
	test(strcmp(parse_ok("SHOW FDS"), "(show fds)") == 0);
}

static void test_show_is_paused(void)
{
	test(strcmp(parse_ok("SHOW IS_PAUSED"), "(show is_paused)") == 0);
}

static void test_show_host_utilization(void)
{
	test(strcmp(parse_ok("SHOW HOST_UTILIZATION"),
		    "(show host_utilization)") == 0);
}

static void test_show_rules(void)
{
	test(strcmp(parse_ok("SHOW RULES"), "(show rules)") == 0);
}

static void test_show_config(void)
{
	test(strcmp(parse_ok("SHOW CONFIG"), "(show config)") == 0);
}

static void test_show_case_insensitive(void)
{
	test(strcmp(parse_ok("show stats"), "(show stats)") == 0);
	test(strcmp(parse_ok("Show Stats"), "(show stats)") == 0);
	test(strcmp(parse_ok("SHOW STATS;"), "(show stats)") == 0);
}

static void test_show_whitespace(void)
{
	test(strcmp(parse_ok("  SHOW   STATS  "), "(show stats)") == 0);
}

static void test_show_with_block_comment(void)
{
	test(strcmp(parse_ok("SHOW /* comment */ STATS"), "(show stats)") == 0);
}

/*
 * KILL_CLIENT tests
 */
static void test_kill_client_basic(void)
{
	const char *res = parse_ok("KILL_CLIENT c3f1a2b4e890c");
	test(strcmp(res, "(kill-client c3f1a2b4e890c)") == 0);
}

static void test_kill_client_case_insensitive_cmd(void)
{
	test(strcmp(parse_ok("kill_client c3f1a2b4e890c"),
		    "(kill-client c3f1a2b4e890c)") == 0);
}

static void test_kill_client_with_semicolon(void)
{
	test(strcmp(parse_ok("KILL_CLIENT c3f1a2b4e890c;"),
		    "(kill-client c3f1a2b4e890c)") == 0);
}

static void test_kill_client_bad_id_length(void)
{
	parse_fail("KILL_CLIENT c3f1");
	parse_fail("KILL_CLIENT c3f1a2b4e890cd");
	parse_fail("KILL_CLIENT");
}

/*
 * Admin command tests
 */
static void test_reload(void)
{
	test(strcmp(parse_ok("RELOAD"), "(reload)") == 0);
	test(strcmp(parse_ok("reload"), "(reload)") == 0);
	test(strcmp(parse_ok("RELOAD;"), "(reload)") == 0);
}

static void test_pause(void)
{
	test(strcmp(parse_ok("PAUSE"), "(pause)") == 0);
	test(strcmp(parse_ok("pause"), "(pause)") == 0);
	test(strcmp(parse_ok("PAUSE;"), "(pause)") == 0);
}

static void test_resume(void)
{
	test(strcmp(parse_ok("RESUME"), "(resume)") == 0);
	test(strcmp(parse_ok("resume"), "(resume)") == 0);
	test(strcmp(parse_ok("RESUME;"), "(resume)") == 0);
}

static void test_gc(void)
{
	test(strcmp(parse_ok("GC"), "(gc)") == 0);
	test(strcmp(parse_ok("gc"), "(gc)") == 0);
	test(strcmp(parse_ok("GC;"), "(gc)") == 0);
}

static void test_set_equals(void)
{
	test(strcmp(parse_ok("SET key = value"), "(set key=value)") == 0);
}

static void test_set_to(void)
{
	test(strcmp(parse_ok("SET key TO value"), "(set key=value)") == 0);
}

static void test_set_string_value(void)
{
	test(strcmp(parse_ok("SET key = 'myvalue'"), "(set key=myvalue)") == 0);
}

static void test_set_default(void)
{
	test(strcmp(parse_ok("SET key = DEFAULT"), "(set key=default)") == 0);
	test(strcmp(parse_ok("SET key TO DEFAULT"), "(set key=default)") == 0);
}

static void test_set_integer(void)
{
	test(strcmp(parse_ok("SET key = 42"), "(set key=42)") == 0);
}

static void test_set_with_semicolon(void)
{
	test(strcmp(parse_ok("SET key = value;"), "(set key=value)") == 0);
}

static void test_drop_servers(void)
{
	test(strcmp(parse_ok("DROP SERVERS"), "(drop servers)") == 0);
	test(strcmp(parse_ok("drop servers"), "(drop servers)") == 0);
	test(strcmp(parse_ok("DROP SERVERS;"), "(drop servers)") == 0);
}

static void test_empty_input(void)
{
	od_linear_alloc_reset(&s_arena);
	od_console_node_t *node = od_console_parse("", 0, &s_arena, NULL, NULL);
	test(node == NULL);
}

static void test_parse_errors(void)
{
	parse_fail("SELECT 1");
	parse_fail("SHOW");
	parse_fail("KILL_CLIENT");
	parse_fail("RELOAD extra");
	parse_fail("PAUSE extra");
	parse_fail("RESUME extra");
	parse_fail("GC extra");
	parse_fail("SET");
	parse_fail("SET key");
	parse_fail("DROP");
	parse_fail("DROP UNKNOWN");
	parse_fail("COMMIT");
	parse_fail("BEGIN");
	parse_fail("DEALLOCATE ALL");
	parse_fail("DISCARD ALL");
}

void odyssey_test_console_parser(void)
{
	od_linear_alloc_init(&s_arena, s_arena_buf, sizeof(s_arena_buf));

	test_show_stats();
	test_show_help();
	test_show_pools();
	test_show_pools_extended();
	test_show_databases();
	test_show_servers();
	test_show_server_prep_stmts();
	test_show_global_prepared_stmts();
	test_show_clients();
	test_show_lists();
	test_show_instance();
	test_show_errors();
	test_show_errors_per_route();
	test_show_version();
	test_show_listen();
	test_show_storages();
	test_show_fds();
	test_show_is_paused();
	test_show_host_utilization();
	test_show_rules();
	test_show_config();
	test_show_case_insensitive();
	test_show_whitespace();
	test_show_with_block_comment();

	test_kill_client_basic();
	test_kill_client_case_insensitive_cmd();
	test_kill_client_with_semicolon();
	test_kill_client_bad_id_length();

	test_reload();
	test_pause();
	test_resume();
	test_gc();

	test_set_equals();
	test_set_to();
	test_set_string_value();
	test_set_default();
	test_set_integer();
	test_set_with_semicolon();

	test_drop_servers();

	test_empty_input();
	test_parse_errors();
}

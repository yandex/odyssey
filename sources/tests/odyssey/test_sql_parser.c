#include <odyssey.h>
#include <tests/odyssey_test.h>

#include <sql/minimal/ast.h>
#include <sql/minimal/parser.h>
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
	od_sql_minimal_node_t *node = od_sql_minimal_parse(
		input, strlen(input), &s_arena, on_error, err);

	if (node == NULL || strlen(err) != 0) {
		fprintf(stderr,
			"parse_ok failed: input=[%s] err=[s%s] node=%p\n",
			input, err, (void *)node);
		abort();
	}

	int n = od_sql_minimal_node_print(node, out, sizeof(out));
	test(n > 0 && (size_t)n < sizeof(out));
	return out;
}

static void parse_fail(const char *input)
{
	static char err[PRINT_BUF_SIZE];

	memset(err, 0, sizeof(err));
	od_linear_alloc_reset(&s_arena);
	od_sql_minimal_node_t *node = od_sql_minimal_parse(
		input, strlen(input), &s_arena, on_error, err);
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
static void test_show_basic(void)
{
	test(strcmp(parse_ok("SHOW search_path"), "(show search_path)") == 0);
}

static void test_show_all(void)
{
	test(strcmp(parse_ok("SHOW ALL"), "(show all)") == 0);
}

static void test_show_dotted(void)
{
	test(strcmp(parse_ok("SHOW odyssey.target_session_attrs"),
		    "(show odyssey.target_session_attrs)") == 0);
}

static void test_show_case_insensitive(void)
{
	test(strcmp(parse_ok("show search_path"), "(show search_path)") == 0);
	test(strcmp(parse_ok("Show Search_Path"), "(show search_path)") == 0);
}

static void test_show_with_semicolon(void)
{
	test(strcmp(parse_ok("SHOW search_path;"), "(show search_path)") == 0);
}

static void test_show_whitespace(void)
{
	test(strcmp(parse_ok("  SHOW   search_path  "), "(show search_path)") ==
	     0);
}

static void test_show_with_block_comment(void)
{
	test(strcmp(parse_ok("SHOW /* comment */ search_path"),
		    "(show search_path)") == 0);
}

static void test_show_multiline_block_comment(void)
{
	test(strcmp(parse_ok("/* multi\nline\ncomment */ SHOW search_path"),
		    "(show search_path)") == 0);
}

static void test_set_equals(void)
{
	test(strcmp(parse_ok("SET search_path = public"),
		    "(set search_path=public)") == 0);
}

static void test_set_to(void)
{
	test(strcmp(parse_ok("SET search_path TO public"),
		    "(set search_path=public)") == 0);
}

static void test_set_string_value(void)
{
	test(strcmp(parse_ok("SET application_name = 'myapp'"),
		    "(set application_name=myapp)") == 0);
}

static void test_set_default(void)
{
	test(strcmp(parse_ok("SET search_path = DEFAULT"),
		    "(set search_path=default)") == 0);
	test(strcmp(parse_ok("SET search_path TO DEFAULT"),
		    "(set search_path=default)") == 0);
}

static void test_set_session_local(void)
{
	test(strcmp(parse_ok("SET SESSION search_path = public"),
		    "(set search_path=public)") == 0);
	test(strcmp(parse_ok("SET LOCAL search_path = public"),
		    "(set search_path=public)") == 0);
}

static void test_set_dotted_name(void)
{
	test(strcmp(parse_ok("SET odyssey.target_session_attrs = 'read-only'"),
		    "(set odyssey.target_session_attrs=read-only)") == 0);
}

static void test_set_boolean_values(void)
{
	test(strcmp(parse_ok("SET enable_seqscan = ON"),
		    "(set enable_seqscan=on)") == 0);
	test(strcmp(parse_ok("SET enable_seqscan = OFF"),
		    "(set enable_seqscan=off)") == 0);
	test(strcmp(parse_ok("SET enable_seqscan = TRUE"),
		    "(set enable_seqscan=true)") == 0);
	test(strcmp(parse_ok("SET enable_seqscan = FALSE"),
		    "(set enable_seqscan=false)") == 0);
}

static void test_set_integer(void)
{
	test(strcmp(parse_ok("SET work_mem = 4096"), "(set work_mem=4096)") ==
	     0);
}

static void test_set_with_semicolon(void)
{
	test(strcmp(parse_ok("SET search_path = public;"),
		    "(set search_path=public)") == 0);
}

static void test_console_show_stats(void)
{
	test(strcmp(parse_ok("SHOW STATS"), "(show stats)") == 0);
}

static void test_console_show_servers(void)
{
	test(strcmp(parse_ok("SHOW SERVERS"), "(show servers)") == 0);
}

static void test_console_show_server_prep_stmts(void)
{
	test(strcmp(parse_ok("SHOW SERVER_PREP_STMTS"),
		    "(show server_prep_stmts)") == 0);
}

static void test_console_show_clients(void)
{
	test(strcmp(parse_ok("SHOW CLIENTS"), "(show clients)") == 0);
}

static void test_console_show_lists(void)
{
	test(strcmp(parse_ok("SHOW LISTS"), "(show lists)") == 0);
}

static void test_console_show_help(void)
{
	test(strcmp(parse_ok("SHOW HELP"), "(show help)") == 0);
}

static void test_console_show_pools(void)
{
	test(strcmp(parse_ok("SHOW POOLS"), "(show pools)") == 0);
}

static void test_console_show_pools_extended(void)
{
	test(strcmp(parse_ok("SHOW POOLS_EXTENDED"), "(show pools_extended)") ==
	     0);
}

static void test_console_show_databases(void)
{
	test(strcmp(parse_ok("SHOW DATABASES"), "(show databases)") == 0);
}

static void test_console_show_errors(void)
{
	test(strcmp(parse_ok("SHOW ERRORS"), "(show errors)") == 0);
}

static void test_console_show_errors_per_route(void)
{
	test(strcmp(parse_ok("SHOW ERRORS_PER_ROUTE"),
		    "(show errors_per_route)") == 0);
}

static void test_console_show_version(void)
{
	test(strcmp(parse_ok("SHOW VERSION"), "(show version)") == 0);
	test(strcmp(parse_ok("SHOW VERSION_EXTENDED"),
		    "(show version_extended)") == 0);
}

static void test_console_show_misc(void)
{
	test(strcmp(parse_ok("SHOW FRONTEND"), "(show frontend)") == 0);
	test(strcmp(parse_ok("SHOW ROUTER"), "(show router)") == 0);
	test(strcmp(parse_ok("SHOW LISTEN"), "(show listen)") == 0);
	test(strcmp(parse_ok("SHOW STORAGES"), "(show storages)") == 0);
	test(strcmp(parse_ok("SHOW FDS"), "(show fds)") == 0);
	test(strcmp(parse_ok("SHOW IS_PAUSED"), "(show is_paused)") == 0);
	test(strcmp(parse_ok("SHOW HOST_UTILIZATION"),
		    "(show host_utilization)") == 0);
	test(strcmp(parse_ok("SHOW RULES"), "(show rules)") == 0);
}

static void test_console_show_case_insensitive(void)
{
	test(strcmp(parse_ok("show stats"), "(show stats)") == 0);
	test(strcmp(parse_ok("Show Stats"), "(show stats)") == 0);
	test(strcmp(parse_ok("SHOW STATS;"), "(show stats)") == 0);
}

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

static void test_drop_servers(void)
{
	test(strcmp(parse_ok("DROP SERVERS"), "(drop servers)") == 0);
	test(strcmp(parse_ok("drop servers"), "(drop servers)") == 0);
	test(strcmp(parse_ok("DROP SERVERS;"), "(drop servers)") == 0);
}

static void test_begin_basic(void)
{
	test(strcmp(parse_ok("BEGIN"), "(begin)") == 0);
}

static void test_begin_work(void)
{
	test(strcmp(parse_ok("BEGIN WORK"), "(begin)") == 0);
}

static void test_begin_transaction(void)
{
	test(strcmp(parse_ok("BEGIN TRANSACTION"), "(begin)") == 0);
}

static void test_begin_case_insensitive(void)
{
	test(strcmp(parse_ok("begin"), "(begin)") == 0);
	test(strcmp(parse_ok("Begin"), "(begin)") == 0);
}

static void test_begin_with_semicolon(void)
{
	test(strcmp(parse_ok("BEGIN;"), "(begin)") == 0);
}

static void test_deallocate_name(void)
{
	test(strcmp(parse_ok("DEALLOCATE foo_stmt"), "(deallocate foo_stmt)") ==
	     0);
}

static void test_deallocate_prepare_name(void)
{
	test(strcmp(parse_ok("DEALLOCATE PREPARE foo_stmt"),
		    "(deallocate foo_stmt)") == 0);
}

static void test_deallocate_all(void)
{
	test(strcmp(parse_ok("DEALLOCATE ALL"), "(deallocate all)") == 0);
}

static void test_deallocate_prepare_all(void)
{
	test(strcmp(parse_ok("DEALLOCATE PREPARE ALL"), "(deallocate all)") ==
	     0);
}

static void test_deallocate_case_insensitive(void)
{
	test(strcmp(parse_ok("deallocate foo_stmt"), "(deallocate foo_stmt)") ==
	     0);
	test(strcmp(parse_ok("Deallocate All"), "(deallocate all)") == 0);
}

static void test_deallocate_with_semicolon(void)
{
	test(strcmp(parse_ok("DEALLOCATE foo_stmt;"),
		    "(deallocate foo_stmt)") == 0);
	test(strcmp(parse_ok("DEALLOCATE ALL;"), "(deallocate all)") == 0);
}

static void test_discard_all(void)
{
	test(strcmp(parse_ok("DISCARD ALL"), "(discard all)") == 0);
}

static void test_discard_temp(void)
{
	test(strcmp(parse_ok("DISCARD TEMP"), "(discard temp)") == 0);
}

static void test_discard_temporary(void)
{
	test(strcmp(parse_ok("DISCARD TEMPORARY"), "(discard temp)") == 0);
}

static void test_discard_plans(void)
{
	test(strcmp(parse_ok("DISCARD PLANS"), "(discard plans)") == 0);
}

static void test_discard_sequences(void)
{
	test(strcmp(parse_ok("DISCARD SEQUENCES"), "(discard sequences)") == 0);
}

static void test_discard_case_insensitive(void)
{
	test(strcmp(parse_ok("discard all"), "(discard all)") == 0);
	test(strcmp(parse_ok("Discard Plans"), "(discard plans)") == 0);
}

static void test_discard_with_semicolon(void)
{
	test(strcmp(parse_ok("DISCARD ALL;"), "(discard all)") == 0);
}

static void test_empty_input(void)
{
	od_linear_alloc_reset(&s_arena);
	od_sql_minimal_node_t *node =
		od_sql_minimal_parse("", 0, &s_arena, NULL, NULL);
	test(node == NULL);
}

static void test_parse_errors(void)
{
	parse_fail("SELECT 1");
	parse_fail("SHOW");
	parse_fail("SET foo =");
	parse_fail("DROP");
	parse_fail("CREATE MODULE");
	parse_fail("DEALLOCATE");
	parse_fail("DEALLOCATE PREPARE");
	parse_fail("DISCARD");
	parse_fail("COMMIT");
	parse_fail("ROLLBACK");
	parse_fail("ABORT");
	parse_fail("END");
}

/*
 * the point is to verify that flex does not call exit(1) when
 * yy_scan_bytes cannot allocate the buffer copy — instead the parser
 * should return NULL via longjmp recovery
 */
static void test_long_query_oom(void)
{
	static char big_query[ARENA_SIZE * 2];
	memset(big_query, ' ', sizeof(big_query) - 1);
	memcpy(big_query, "SELECT ", 7);
	big_query[sizeof(big_query) - 2] = ';';
	big_query[sizeof(big_query) - 1] = '\0';

	od_linear_alloc_reset(&s_arena);
	od_sql_minimal_error_cb_t saved_cb = NULL;
	(void)saved_cb;

	od_sql_minimal_node_t *node = od_sql_minimal_parse(
		big_query, strlen(big_query), &s_arena, NULL, NULL);
	test(node == NULL);
}

static void test_long_set_value(void)
{
	static char long_value[512];
	memset(long_value, 'x', sizeof(long_value) - 1);
	long_value[sizeof(long_value) - 1] = '\0';

	char input[600];
	int n = snprintf(input, sizeof(input), "SET application_name = '%s'",
			 long_value);
	test(n > 0 && (size_t)n < sizeof(input));

	const char *res = parse_ok(input);
	test(strncmp(res, "(set application_name=", 21) == 0);
	test(strstr(res, "xxx") != NULL);
}

void odyssey_test_sql_minimal_parser(void)
{
	od_linear_alloc_init(&s_arena, s_arena_buf, sizeof(s_arena_buf));

	test_show_basic();
	test_show_all();
	test_show_dotted();
	test_show_case_insensitive();
	test_show_with_semicolon();
	test_show_whitespace();
	test_show_with_block_comment();
	test_show_multiline_block_comment();

	test_set_equals();
	test_set_to();
	test_set_string_value();
	test_set_default();
	test_set_session_local();
	test_set_dotted_name();
	test_set_boolean_values();
	test_set_integer();
	test_set_with_semicolon();

	test_console_show_stats();
	test_console_show_servers();
	test_console_show_server_prep_stmts();
	test_console_show_clients();
	test_console_show_lists();
	test_console_show_help();
	test_console_show_pools();
	test_console_show_pools_extended();
	test_console_show_databases();
	test_console_show_errors();
	test_console_show_errors_per_route();
	test_console_show_version();
	test_console_show_misc();
	test_console_show_case_insensitive();

	test_kill_client_basic();
	test_kill_client_case_insensitive_cmd();
	test_kill_client_with_semicolon();
	test_kill_client_bad_id_length();

	test_reload();
	test_pause();
	test_resume();

	test_drop_servers();

	test_begin_basic();
	test_begin_work();
	test_begin_transaction();
	test_begin_case_insensitive();
	test_begin_with_semicolon();

	test_deallocate_name();
	test_deallocate_prepare_name();
	test_deallocate_all();
	test_deallocate_prepare_all();
	test_deallocate_case_insensitive();
	test_deallocate_with_semicolon();

	test_discard_all();
	test_discard_temp();
	test_discard_temporary();
	test_discard_plans();
	test_discard_sequences();
	test_discard_case_insensitive();
	test_discard_with_semicolon();

	test_empty_input();
	test_parse_errors();

	test_long_query_oom();
	test_long_set_value();
}

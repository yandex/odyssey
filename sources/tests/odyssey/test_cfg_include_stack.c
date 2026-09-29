#include <odyssey.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <machinarium/machinarium.h>

#include <cfg/diag.h>
#include <cfg/model.h>
#include <cfg/reader.h>
#include <tests/odyssey_test.h>

/*
 * Reload parses config on the system coroutine (128 KiB with 4 KiB pages). Nested
 * include must not require a recursive yyparse frame per file.
 */
#define TEST_INCLUDE_LIMIT OD_CFG_MAX_INCLUDE_DEPTH

static const char *k_valid_body = "listen {\n"
				  "	host \"127.0.0.1\"\n"
				  "	port 6432\n"
				  "}\n"
				  "storage \"local\" {\n"
				  "	type \"local\"\n"
				  "}\n"
				  "database \"console\" {\n"
				  "	user default {\n"
				  "		authentication \"none\"\n"
				  "		storage \"local\"\n"
				  "		pool \"session\"\n"
				  "	}\n"
				  "}\n";

struct parse_job {
	const char *path;
	int rc;
	char zone[64];
	char err[256];
	char err_file[256];
	int err_line;
	int err_column;
};

static void write_file(const char *path, const char *content)
{
	FILE *file = fopen(path, "w");
	test(file != NULL);
	test(fputs(content, file) != EOF);
	test(fclose(file) == 0);
}

static void path_join(char *out, size_t out_size, const char *dir,
		      const char *name)
{
	int n = snprintf(out, out_size, "%s/%s", dir, name);
	test(n > 0 && (size_t)n < out_size);
}

static void write_include_chain(const char *dir, int depth, const char *leaf)
{
	char path[512];
	char next[512];
	char buf[1024];
	int n;

	test(depth >= 1);

	path_join(next, sizeof(next), dir, "i1.conf");
	n = snprintf(buf, sizeof(buf), "include \"%s\"\n%s", next,
		     k_valid_body);
	test(n > 0 && (size_t)n < sizeof(buf));
	path_join(path, sizeof(path), dir, "top.conf");
	write_file(path, buf);

	for (int i = 1; i < depth; i++) {
		char name[32];
		char next_name[32];

		n = snprintf(name, sizeof(name), "i%d.conf", i);
		test(n > 0 && (size_t)n < sizeof(name));
		n = snprintf(next_name, sizeof(next_name), "i%d.conf", i + 1);
		test(n > 0 && (size_t)n < sizeof(next_name));
		path_join(next, sizeof(next), dir, next_name);
		n = snprintf(buf, sizeof(buf), "include \"%s\"\n", next);
		test(n > 0 && (size_t)n < sizeof(buf));
		path_join(path, sizeof(path), dir, name);
		write_file(path, buf);
	}

	char leaf_name[32];
	n = snprintf(leaf_name, sizeof(leaf_name), "i%d.conf", depth);
	test(n > 0 && (size_t)n < sizeof(leaf_name));
	path_join(path, sizeof(path), dir, leaf_name);
	write_file(path, leaf);
}

static void remove_include_chain(const char *dir, int depth)
{
	char path[512];
	char name[32];

	path_join(path, sizeof(path), dir, "top.conf");
	unlink(path);

	for (int i = 1; i <= depth; i++) {
		int n = snprintf(name, sizeof(name), "i%d.conf", i);
		test(n > 0 && (size_t)n < sizeof(name));
		path_join(path, sizeof(path), dir, name);
		unlink(path);
	}

	rmdir(dir);
}

static void parse_on_system_coro(void *arg)
{
	struct parse_job *job = arg;
	od_cfg_model_t model;
	od_cfg_diag_list_t diags;

	od_cfg_model_init(&model);
	od_cfg_diag_list_init(&diags);

	job->rc = od_cfg_parse_file(job->path, &model, &diags);
	if (model.global.availability_zone.value != NULL) {
		snprintf(job->zone, sizeof(job->zone), "%s",
			 model.global.availability_zone.value);
	}
	if (diags.count > 0 && diags.items[0].message != NULL) {
		snprintf(job->err, sizeof(job->err), "%s",
			 diags.items[0].message);
		if (diags.items[0].location.filename != NULL) {
			snprintf(job->err_file, sizeof(job->err_file), "%s",
				 diags.items[0].location.filename);
		}
		job->err_line = diags.items[0].location.first_line;
		job->err_column = diags.items[0].location.first_column;
	}

	od_cfg_diag_list_free(&diags);
	od_cfg_model_free(&model);
	machine_stop_current();
}

static void run_parse_on_system_stack(struct parse_job *job)
{
	memset(job->zone, 0, sizeof(job->zone));
	memset(job->err, 0, sizeof(job->err));
	memset(job->err_file, 0, sizeof(job->err_file));
	job->err_line = 0;
	job->err_column = 0;
	job->rc = 1;

	long page = sysconf(_SC_PAGESIZE);
	test(page > 0);
	int pages = (int)(128 * 1024 / page);
	test(pages >= 1);
	machinarium_set_stack_size(16);
	machinarium_set_system_stack_size(pages);
	test(machinarium_init() == 0);

	int64_t id = machine_create("cfg-include", parse_on_system_coro, job);
	test(id != -1);
	test(machine_wait(id) != -1);

	machinarium_free();
}

static char *make_temp_dir(char *dir, size_t dir_size)
{
	int n = snprintf(dir, dir_size, "/tmp/odyssey-cfg-include-XXXXXX");
	test(n > 0 && (size_t)n < dir_size);
	test(mkdtemp(dir) != NULL);
	return dir;
}

static void test_include_limit_on_system_stack(void)
{
	char dir[128];
	char top[512];
	struct parse_job job;

	make_temp_dir(dir, sizeof(dir));
	write_include_chain(dir, TEST_INCLUDE_LIMIT,
			    "availability_zone \"nested\"\n");
	path_join(top, sizeof(top), dir, "top.conf");

	job.path = top;
	run_parse_on_system_stack(&job);

	if (job.rc != 0) {
		fprintf(stdout, "parse failed: %s (%s)\n", job.err,
			job.err_file);
	}
	test(job.rc == 0);
	test(strcmp(job.zone, "nested") == 0);

	remove_include_chain(dir, TEST_INCLUDE_LIMIT);
}

static void test_sibling_includes_on_system_stack(void)
{
	char dir[128];
	char top[512];
	char child[512];
	struct parse_job job;

	make_temp_dir(dir, sizeof(dir));
	path_join(top, sizeof(top), dir, "top.conf");
	path_join(child, sizeof(child), dir, "i1.conf");
	write_file(child, "");
	FILE *file = fopen(top, "w");
	test(file != NULL);
	test(fputs(k_valid_body, file) != EOF);
	for (int i = 0; i <= TEST_INCLUDE_LIMIT; i++) {
		test(fprintf(file, "include \"%s\"\n", child) > 0);
	}
	test(fputs("availability_zone \"after-includes\"\n", file) != EOF);
	test(fclose(file) == 0);

	job.path = top;
	run_parse_on_system_stack(&job);

	test(job.rc == 0);
	test(strcmp(job.zone, "after-includes") == 0);

	remove_include_chain(dir, 1);
}

static void test_include_past_limit_on_system_stack(void)
{
	char dir[128];
	char top[512];
	struct parse_job job;

	make_temp_dir(dir, sizeof(dir));
	write_include_chain(dir, TEST_INCLUDE_LIMIT + 1,
			    "availability_zone \"too-deep\"\n");
	path_join(top, sizeof(top), dir, "top.conf");

	job.path = top;
	run_parse_on_system_stack(&job);

	test(job.rc != 0);
	test(strstr(job.err, "include depth limit") != NULL);
	test(job.zone[0] == 0);

	remove_include_chain(dir, TEST_INCLUDE_LIMIT + 1);
}

static void test_missing_include_on_system_stack(void)
{
	char dir[128];
	char top[512];
	char missing[512];
	char buf[1024];
	struct parse_job job;
	int n;

	make_temp_dir(dir, sizeof(dir));
	path_join(missing, sizeof(missing), dir, "missing.conf");
	n = snprintf(buf, sizeof(buf), "include \"%s\"\n", missing);
	test(n > 0 && (size_t)n < sizeof(buf));
	write_include_chain(dir, 1, buf);
	path_join(top, sizeof(top), dir, "top.conf");

	job.path = top;
	run_parse_on_system_stack(&job);

	test(job.rc != 0);
	test(strstr(job.err, "failed to open config file") != NULL);
	test(strcmp(job.err_file, missing) == 0);
	test(job.err_line == 1);
	test(job.err_column == 1);

	remove_include_chain(dir, 1);
}

static void test_include_file_boundaries(void)
{
	char dir[128];
	char top[512];
	char child[512];
	struct parse_job job;
	const struct {
		const char *child;
		const char *parent_tail;
	} cases[] = {
		{ "availability_zone\n", "\"cross-file\"\n" },
		{ "listen {\n", "host \"127.0.0.1\" port 6433 }\n" },
	};

	make_temp_dir(dir, sizeof(dir));
	path_join(top, sizeof(top), dir, "top.conf");
	path_join(child, sizeof(child), dir, "i1.conf");
	job.path = top;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		write_file(child, cases[i].child);
		FILE *file = fopen(top, "w");
		test(file != NULL);
		test(fprintf(file, "include \"%s\"\n%s%s", child,
			     cases[i].parent_tail, k_valid_body) > 0);
		test(fclose(file) == 0);

		run_parse_on_system_stack(&job);
		test(job.rc != 0);
		test(strstr(job.err, "syntax error") != NULL);
		test(strcmp(job.err_file, child) == 0);
		test(job.err_line == 2);
		test(job.err_column == 1);
	}

	write_file(child, "log_debug yes\n");
	FILE *file = fopen(top, "w");
	test(file != NULL);
	test(fprintf(file, "include \"%s\"\n\ninvalid_directive yes\n", child) >
	     0);
	test(fclose(file) == 0);
	run_parse_on_system_stack(&job);
	test(job.rc != 0);
	test(strstr(job.err, "syntax error") != NULL);
	test(strcmp(job.err_file, top) == 0);
	test(job.err_line == 3);
	test(job.err_column == 1);

	remove_include_chain(dir, 1);
}

void odyssey_test_cfg_include_stack(void)
{
	test_include_limit_on_system_stack();
	test_sibling_includes_on_system_stack();
	test_include_past_limit_on_system_stack();
	test_missing_include_on_system_stack();
	test_include_file_boundaries();
}

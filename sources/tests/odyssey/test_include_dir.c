/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include <cfg/reader.h>
#include <tests/odyssey_test.h>

#include <sys/stat.h>

static const struct {
	const char *name;
	const char *contents; /* NULL creates a directory. */
} entries[] = {
	{ "parts", NULL },
	{ "parts/a.conf", "listen { port 6402 }\n" },
	{ "parts/Z.conf", "include_dir \"nested\"\n" },
	{ "parts/nested", NULL },
	{ "parts/nested/port.conf", "listen { port 6401 }\n" },
	{ "parts/.hidden.conf", "invalid option\n" },
	{ "parts/disabled.conf.bak", "invalid option\n" },
	{ "parts/subdir.conf", NULL },
	{ "parts/subdir.conf/skip.conf", "invalid option\n" },
	{ "empty", NULL },
	{ "main.conf", "storage \"local\" { type \"local\" }\n"
		       "listen { port 6400 }\n"
		       "include_dir \"parts\"\n"
		       "include_dir \"empty\"\n"
		       "listen { port 6403 }\n" },
	{ "missing.conf", "include_dir \"absent\"\n" },
	{ "duplicate.conf", "workers 2\ninclude_dir \"duplicates\"\n" },
	{ "duplicates", NULL },
	{ "duplicates/workers.conf", "workers 3\n" },
	{ "overlay.conf", "workers 2\n" },
	{ "overlay.conf.autoconf", "include_dir \"empty\"\n" },
	{ "loop", NULL },
	{ "loop/again.conf", "include_dir \".\"\n" },
	{ "cycle.conf", "include_dir \"loop\"\n" },
#ifdef OD_CONFIG_YAML
	{ "mixed", NULL },
	{ "mixed/c.yml", "listen: [{port: 6413}]\n" },
	{ "mixed/b.yaml", "listen: [{port: 6412}]\n" },
	{ "mixed/a.json", "{\"listen\":[{\"port\":6411}]}\n" },
	{ "mixed.yaml", "include_dir: mixed\n"
			"storage: [{name: local, type: local}]\n" },
#endif
};

static void entry_path(char *path, size_t size, const char *root,
		       const char *name)
{
	int n = snprintf(path, size, "%s/%s", root, name);
	test(n >= 0 && (size_t)n < size);
}

static int parse_config(const char *root, const char *name,
			od_cfg_model_t *model, od_cfg_diag_list_t *diags)
{
	char path[256];
	entry_path(path, sizeof(path), root, name);
	od_cfg_model_init(model);
	od_cfg_diag_list_init(diags);
	return od_cfg_parse_file_with_autoconf(path, NULL, model, diags);
}

static void check_ports(const char *root, const char *name, const int *ports,
			size_t count)
{
	od_cfg_model_t model;
	od_cfg_diag_list_t diags;
	int rc = parse_config(root, name, &model, &diags);
	if (rc != 0) {
		for (size_t i = 0; i < diags.count; i++) {
			fprintf(stderr, "%s: %s\n",
				diags.items[i].location.filename,
				diags.items[i].message);
		}
	}
	test(rc == 0);
	test(model.listens_count == count);
	for (size_t i = 0; i < count; ++i) {
		test(model.listens[i]->port.value == ports[i]);
	}
	od_cfg_diag_list_free(&diags);
	od_cfg_model_free(&model);
}

static void check_error(const char *root, const char *name, const char *message,
			const char *error_file)
{
	od_cfg_model_t model;
	od_cfg_diag_list_t diags;
	test(parse_config(root, name, &model, &diags) != 0);
	test(od_cfg_diag_has_errors(&diags));
	test(strstr(diags.items[0].message, message) != NULL);
	if (error_file != NULL) {
		char path[256];
		entry_path(path, sizeof(path), root, error_file);
		test(strcmp(diags.items[0].location.filename, path) == 0);
	}
	od_cfg_diag_list_free(&diags);
	od_cfg_model_free(&model);
}

void odyssey_test_include_dir(void)
{
	char root[] = "/tmp/odyssey-include-dir-XXXXXX";
	test(mkdtemp(root) != NULL);
	char path[256];
	for (size_t i = 0; i < lengthof(entries); i++) {
		entry_path(path, sizeof(path), root, entries[i].name);
		if (entries[i].contents == NULL) {
			test(mkdir(path, 0700) == 0);
		} else {
			FILE *file = fopen(path, "wx");
			test(file != NULL);
			test(fputs(entries[i].contents, file) >= 0);
			test(fclose(file) == 0);
		}
	}

	const int ports[] = { 6400, 6401, 6402, 6403 };
	check_ports(root, "main.conf", ports, lengthof(ports));
	check_error(root, "missing.conf", "absent", NULL);
	check_error(root, "duplicate.conf", "duplicate option 'workers'",
		    "duplicates/workers.conf");
	check_error(root, "overlay.conf", "forbidden", "overlay.conf.autoconf");
	check_error(root, "cycle.conf", "depth", NULL);
#ifdef OD_CONFIG_YAML
	const int mixed_ports[] = { 6411, 6412, 6413 };
	check_ports(root, "mixed.yaml", mixed_ports, lengthof(mixed_ports));
#endif

	for (size_t i = lengthof(entries); i > 0; i--) {
		entry_path(path, sizeof(path), root, entries[i - 1].name);
		test(remove(path) == 0);
	}
	test(rmdir(root) == 0);
}

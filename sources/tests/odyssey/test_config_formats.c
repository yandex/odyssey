/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include <cfg/reader.h>
#include <tests/odyssey_test.h>

static const char native_config[] =
	"workers 3\n"
	"log_debug yes\n"
	"stats_interval \"1.5s\"\n"
	"listen { host \"127.0.0.1\" port 6433 }\n"
	"storage \"s\" { type \"remote\" host \"db.internal\" port 5433 }\n"
	"database default { user default { storage \"s\" pool \"transaction\" "
	"authentication \"none\" } }\n";

#ifdef OD_CONFIG_YAML
static const char yaml_config[] =
	"workers: 3\n"
	"include: []\n"
	"log_debug: true\n"
	"stats_interval: 1.5s\n"
	"listen:\n"
	"  - host: 127.0.0.1\n"
	"    port: 6433\n"
	"storage:\n"
	"  - {name: s, type: remote, host: db.internal, port: 5433}\n"
	"database:\n"
	"  - default: True\n"
	"    user:\n"
	"      - default: true\n"
	"        storage: s\n"
	"        pool: transaction\n"
	"        authentication: none\n";

static const char json_config[] =
	"{\"workers\":3,\"log_debug\":true,\"stats_interval\":\"1.5s\","
	"\"listen\":[{\"host\":\"127.0.0.1\",\"port\":6433}],"
	"\"storage\":[{\"name\":\"s\",\"type\":\"remote\","
	"\"host\":\"db.internal\",\"port\":5433}],"
	"\"database\":[{\"default\":true,\"user\":[{\"default\":true,"
	"\"storage\":\"s\",\"pool\":\"transaction\",\"authentication\":\"none\"}]}]}";
#endif

static void write_config(char *path, size_t path_size, const char *suffix,
			 const char *contents)
{
	int len = snprintf(path, path_size, "/tmp/odyssey_format_XXXXXX%s",
			   suffix);
	test(len >= 0 && (size_t)len < path_size);
	int fd = mkstemps(path, strlen(suffix));
	test(fd != -1);
	FILE *file = fdopen(fd, "w");
	test(file != NULL);
	test(fputs(contents, file) >= 0);
	test(fclose(file) == 0);
}

static void check_config(const char *suffix, const char *contents)
{
	char path[64];
	write_config(path, sizeof(path), suffix, contents);
	char overlay[sizeof(path) + sizeof(OD_CFG_AUTOCONF_SUFFIX)];
	snprintf(overlay, sizeof(overlay), "%s%s", path,
		 OD_CFG_AUTOCONF_SUFFIX);
	FILE *file = fopen(overlay, "wx");
	test(file != NULL);
	test(fputs("workers 7\n", file) >= 0);
	test(fclose(file) == 0);

	od_cfg_model_t model;
	od_cfg_diag_list_t diags;
	od_cfg_model_init(&model);
	od_cfg_diag_list_init(&diags);
	int rc = od_cfg_parse_file_with_autoconf(path, NULL, &model, &diags);
	test(unlink(overlay) == 0);
	test(unlink(path) == 0);
	if (rc != 0) {
		for (size_t i = 0; i < diags.count; i++) {
			fprintf(stderr, "%s: %s\n", path,
				diags.items[i].message);
		}
	}
	test(rc == 0);
	test(!od_cfg_diag_has_errors(&diags));
	test(model.global.workers.value == 7);
	test(model.global.log_debug.value);
	test(model.global.stats_interval_ms.value == 1500);
	test(model.listens_count == 1);
	test(strcmp(model.listens[0]->host.value, "127.0.0.1") == 0);
	test(model.listens[0]->port.value == 6433);
	test(model.storages_count == 1);
	od_cfg_storage_t *storage = model.storages[0];
	test(strcmp(storage->name, "s") == 0);
	test(strcmp(storage->type.value, "remote") == 0);
	test(strcmp(storage->host.value, "db.internal") == 0);
	test(storage->port.value == 5433);
	test(model.databases_count == 1);
	od_cfg_database_t *db = model.databases[0];
	test(db->is_default);
	test(db->users_count == 1);
	od_cfg_route_t *user = db->users[0];
	test(user->is_default);
	test(strcmp(user->storage.value, "s") == 0);
	test(strcmp(user->pool.value, "transaction") == 0);
	test(strcmp(user->authentication.value, "none") == 0);
	od_cfg_diag_list_free(&diags);
	od_cfg_model_free(&model);
}

#ifdef OD_CONFIG_YAML
static void check_mixed_includes(void)
{
	char json_path[64];
	write_config(json_path, sizeof(json_path), ".json",
		     "{\"database\":[{\"name\":\"default\","
		     "\"user\":[{\"name\":\"default\",\"storage\":\"s\"}]}]}");

	char native_path[64];
	char native[sizeof(native_config) + sizeof(json_path) + 16];
	snprintf(native, sizeof(native), "%sinclude \"%s\"\n", native_config,
		 json_path);
	write_config(native_path, sizeof(native_path), ".conf", native);

	char yaml_path[64];
	char yaml[sizeof(native_path) + 16];
	snprintf(yaml, sizeof(yaml), "include: %s\n", native_path);
	write_config(yaml_path, sizeof(yaml_path), ".yaml", yaml);

	od_cfg_model_t model;
	od_cfg_diag_list_t diags;
	od_cfg_model_init(&model);
	od_cfg_diag_list_init(&diags);
	int rc = od_cfg_parse_file_with_autoconf(yaml_path, NULL, &model,
						 &diags);
	test(unlink(yaml_path) == 0);
	test(unlink(native_path) == 0);
	test(unlink(json_path) == 0);
	test(rc == 0);
	test(!od_cfg_diag_has_errors(&diags));
	test(model.databases_count == 2);
	test(model.databases[0]->is_default);
	test(model.databases[0]->users_count == 1);
	test(model.databases[0]->users[0]->is_default);

	od_cfg_database_t *db = model.databases[1];
	test(!db->is_default && strcmp(db->name, "default") == 0);
	test(strcmp(db->location.filename, json_path) == 0);
	test(db->users_count == 1);
	od_cfg_route_t *user = db->users[0];
	test(!user->is_default && strcmp(user->name, "default") == 0);
	test(strcmp(user->storage.value, "s") == 0);
	od_cfg_diag_list_free(&diags);
	od_cfg_model_free(&model);
}
#endif

static void check_parse_error(const char *suffix, const char *contents,
			      const char *diagnostic)
{
	char path[64];
	write_config(path, sizeof(path), suffix, contents);
	od_cfg_model_t model;
	od_cfg_diag_list_t diags;
	od_cfg_model_init(&model);
	od_cfg_diag_list_init(&diags);
	/* Test parsing independently of the required listen and storage sections. */
	int rc = od_cfg_parse_file_depth(path, &model, &diags, 0, 1);
	test(unlink(path) == 0);
	test(rc != 0);
	test(od_cfg_diag_has_errors(&diags));
	test(strcmp(diags.items[0].location.filename, path) == 0);
	if (diagnostic != NULL) {
		test(strstr(diags.items[0].message, diagnostic) != NULL);
	}
	od_cfg_diag_list_free(&diags);
	od_cfg_model_free(&model);
}

void odyssey_test_config_formats(void)
{
	check_config(".conf", native_config);
#ifdef OD_CONFIG_YAML
	check_config(".yaml", yaml_config);
	check_config(".yml", yaml_config);
	check_config(".json", json_config);
	check_mixed_includes();
	check_parse_error(".yaml", "listen: [{port: 6000, port: 7000}]\n",
			  NULL);
	check_parse_error(".json", "{\"workers\":2,\"workers\":4}", NULL);
	check_parse_error(".yaml", "workers: [\n", NULL);
	check_parse_error(".yaml", "workers: 2\n---\nworkers: 4\n", NULL);
	check_parse_error(".json", "{\"workers\":2} {}", NULL);
	check_parse_error(".json", "workers: 3\n", NULL);
	check_parse_error(".yaml", "workers: [3]\n", NULL);
	check_parse_error(
		".yaml",
		"storage: [{name: s, watchdog: {group_query: SELECT 1}}]\n",
		"group_query");
	check_parse_error(
		".json",
		"{\"database\":[{\"name\":\"db\",\"user\":[{\"name\":\"u\","
		"\"group_query_user\":\"reader\"}]}]}",
		"group_query_user");
#else
	check_parse_error(".yaml", native_config, "BUILD_CONFIG_YAML");
	check_parse_error(".yml", native_config, "BUILD_CONFIG_YAML");
	check_parse_error(".json", native_config, "BUILD_CONFIG_YAML");
#endif
}

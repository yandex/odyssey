/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <status.h>
#include <types.h>
#include <od_memory.h>
#include <util.h>

#include <cfg/scan.h>
#include <cfg/model.h>
#include <cfg/ctx.h>
#include <cfg/reader.h>

#include <unistd.h>

static int read_file(const char *path, char **out, size_t *out_size,
		     od_cfg_diag_list_t *diags)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to open config file '%s': %s", path,
				  strerror(errno));
		return NOT_OK_RESPONSE;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to seek config file '%s': %s", path,
				  strerror(errno));
		fclose(file);
		return NOT_OK_RESPONSE;
	}

	long file_size = ftell(file);
	if (file_size < 0) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to tell config file '%s': %s", path,
				  strerror(errno));
		fclose(file);
		return NOT_OK_RESPONSE;
	}

	if (fseek(file, 0, SEEK_SET) != 0) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to seek config file '%s': %s", path,
				  strerror(errno));
		fclose(file);
		return NOT_OK_RESPONSE;
	}

	char *buf = od_malloc((size_t)file_size + 1);
	if (buf == NULL) {
		od_cfg_diag_error(
			diags, od_cfg_location_empty(path),
			"out of memory while reading config file '%s'", path);
		fclose(file);
		return NOT_OK_RESPONSE;
	}

	size_t read_size = fread(buf, 1, (size_t)file_size, file);
	if (read_size != (size_t)file_size) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to read config file '%s'", path);
		od_free(buf);
		fclose(file);
		return NOT_OK_RESPONSE;
	}

	if (fclose(file) != 0) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to close config file '%s': %s", path,
				  strerror(errno));
		od_free(buf);
		return NOT_OK_RESPONSE;
	}

	buf[file_size] = 0;

	*out = buf;
	*out_size = (size_t)file_size;
	return OK_RESPONSE;
}

#define OD_CFG_MAX_INCLUDE_DEPTH 16
#define OD_CFG_AUTOCONF_SUFFIX ".autoconf"

#define MERGE_PLAIN_FLD(d, s, f)                              \
	do {                                                  \
		if ((s)->f.seen.is_set) {                     \
			od_cfg_seen_free(&(d)->f.seen);       \
			(d)->f.value = (s)->f.value;          \
			(d)->f.seen = (s)->f.seen;            \
			(s)->f.seen.is_set = 0;               \
			(s)->f.seen.location.filename = NULL; \
		}                                             \
	} while (0)

#define MERGE_BOOL(d, s, f) MERGE_PLAIN_FLD(d, s, f)
#define MERGE_INT(d, s, f) MERGE_PLAIN_FLD(d, s, f)
#define MERGE_U64(d, s, f) MERGE_PLAIN_FLD(d, s, f)

#define MERGE_STR(d, s, f)                                    \
	do {                                                  \
		if ((s)->f.seen.is_set) {                     \
			od_cfg_seen_free(&(d)->f.seen);       \
			od_free((d)->f.value);                \
			(d)->f.value = (s)->f.value;          \
			(d)->f.seen = (s)->f.seen;            \
			(s)->f.seen.is_set = 0;               \
			(s)->f.seen.location.filename = NULL; \
			(s)->f.value = NULL;                  \
		}                                             \
	} while (0)

static void od_cfg_global_merge(od_cfg_global_t *dst, od_cfg_global_t *src)
{
	MERGE_BOOL(dst, src, daemonize);
	MERGE_BOOL(dst, src, sequential_routing);
	MERGE_BOOL(dst, src, enable_online_restart);
	MERGE_BOOL(dst, src, virtual_processing);
	MERGE_BOOL(dst, src, virtual_transaction);
	MERGE_BOOL(dst, src, bindwith_reuseport);
	MERGE_BOOL(dst, src, enable_host_watcher);
	MERGE_BOOL(dst, src, log_debug);
	MERGE_BOOL(dst, src, log_to_stdout);
	MERGE_BOOL(dst, src, log_config);
	MERGE_BOOL(dst, src, log_session);
	MERGE_BOOL(dst, src, log_query);
	MERGE_BOOL(dst, src, log_stats);
	MERGE_BOOL(dst, src, log_async);
	MERGE_BOOL(dst, src, log_syslog);
	MERGE_BOOL(dst, src, smart_search_path_enquoting);
	MERGE_BOOL(dst, src, nodelay);
	MERGE_BOOL(dst, src, disable_nolinger);
	MERGE_BOOL(dst, src, log_general_stats_prom);
	MERGE_BOOL(dst, src, log_route_stats_prom);

	MERGE_STR(dst, src, pid_file);
	MERGE_STR(dst, src, unix_socket_dir);
	MERGE_STR(dst, src, unix_socket_mode);
	MERGE_STR(dst, src, locks_dir);
	MERGE_STR(dst, src, external_auth_socket_path);
	MERGE_STR(dst, src, availability_zone);
	MERGE_STR(dst, src, log_file);
	MERGE_STR(dst, src, log_format);
	MERGE_STR(dst, src, log_syslog_ident);
	MERGE_STR(dst, src, log_syslog_facility);
	MERGE_STR(dst, src, cpu_affinity);
	MERGE_STR(dst, src, hba_file);

	MERGE_INT(dst, src, priority);
	MERGE_INT(dst, src, graceful_shutdown_timeout_ms);
	MERGE_INT(dst, src, log_queue_depth);
	MERGE_INT(dst, src, stats_interval);
	MERGE_INT(dst, src, client_max);
	MERGE_INT(dst, src, client_max_routing);
	MERGE_INT(dst, src, accept_rate_limit);
	MERGE_INT(dst, src, server_login_retry);
	MERGE_INT(dst, src, readahead);
	MERGE_INT(dst, src, keepalive);
	MERGE_INT(dst, src, keepalive_keep_interval);
	MERGE_INT(dst, src, keepalive_probes);
	MERGE_INT(dst, src, keepalive_usr_timeout);
	MERGE_INT(dst, src, max_sigterms_to_die);
	MERGE_INT(dst, src, backend_connect_timeout_ms);
	MERGE_INT(dst, src, cancel_timeout_ms);
	MERGE_INT(dst, src, cancel_queue_timeout_ms);
	MERGE_INT(dst, src, cancel_max_inflight);
	MERGE_INT(dst, src, resolvers);
	MERGE_INT(dst, src, dns_cache_ttl);
	MERGE_INT(dst, src, cache_msg_gc_size);
	MERGE_INT(dst, src, cache_msg_gc_count);
	MERGE_INT(dst, src, cache_coroutine);
	MERGE_INT(dst, src, coroutine_stack_size);
	MERGE_INT(dst, src, system_coroutine_stack_size);
	MERGE_INT(dst, src, promhttp_server_port);
	MERGE_INT(dst, src, group_checker_interval);
	MERGE_INT(dst, src, workers);

#undef MERGE_PLAIN_FLD
#undef MERGE_BOOL
#undef MERGE_INT
#undef MERGE_U64
#undef MERGE_STR
}

static int od_cfg_try_autoconf(const char *config_path, od_cfg_model_t *model,
			       od_cfg_diag_list_t *diags)
{
	size_t path_len = strlen(config_path);
	size_t autoconf_path_len =
		path_len + strlen(OD_CFG_AUTOCONF_SUFFIX) + 1;
	char *autoconf_path = od_malloc(autoconf_path_len);
	if (autoconf_path == NULL) {
		od_cfg_diag_error(diags, od_cfg_location_empty(config_path),
				  "out of memory while building autoconf path");
		return -1;
	}

	od_snprintf(autoconf_path, autoconf_path_len, "%s%s", config_path,
		    OD_CFG_AUTOCONF_SUFFIX);

	if (access(autoconf_path, F_OK) != 0) {
		od_free(autoconf_path);
		return 0;
	}

	/* swap in a fresh global so autoconf can set fields without duplicate errors */
	od_cfg_global_t saved_global = model->global;
	memset(&model->global, 0, sizeof(model->global));

	int rc = od_cfg_parse_file_depth(autoconf_path, model, diags, 1, 0);
	od_free(autoconf_path);

	if (rc != 0) {
		/* autoconf failed */
		od_cfg_global_free(&model->global);
		model->global = saved_global;
		return -1;
	}

	/* autoconf values override base values */
	od_cfg_global_merge(&saved_global, &model->global);

	/* free the scratch global (string values already moved) */
	od_cfg_global_free(&model->global);

	model->global = saved_global;
	return 0;
}

int od_cfg_parse_file_depth(const char *path, od_cfg_model_t *model,
			    od_cfg_diag_list_t *diags, int depth,
			    int allow_include)
{
	if (depth > OD_CFG_MAX_INCLUDE_DEPTH) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "include depth limit (%d) exceeded — "
				  "possible circular include",
				  OD_CFG_MAX_INCLUDE_DEPTH);
		return -1;
	}

	char *buf = NULL;
	size_t size = 0;

	int rc = read_file(path, &buf, &size, diags);
	if (rc != 0) {
		return -1;
	}

	od_cfg_parse_ctx_t ctx;
	od_cfg_parse_ctx_init(&ctx, path, buf, size, model, diags);
	ctx.include_depth = depth;
	ctx.allow_include = allow_include;

	yyscan_t scanner;
	if (yylex_init_extra(&ctx, &scanner) != 0) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to initialize config scanner");
		od_cfg_parse_ctx_free(&ctx);
		od_free(buf);
		return -1;
	}

	YY_BUFFER_STATE buffer = yy_scan_bytes(buf, (int)size, scanner);
	if (buffer == NULL) {
		od_cfg_diag_error(diags, od_cfg_location_empty(path),
				  "failed to create config scanner buffer");
		yylex_destroy(scanner);
		od_cfg_parse_ctx_free(&ctx);
		od_free(buf);
		return -1;
	}

	int parse_rc = yyparse(scanner, &ctx);

	yy_delete_buffer(buffer, scanner);
	yylex_destroy(scanner);

	od_cfg_parse_ctx_free(&ctx);
	od_free(buf);

	if (parse_rc != 0 || od_cfg_diag_has_errors(diags)) {
		return -1;
	}

	/* validate only at top level, not for each included file */
	if (depth == 0) {
		/* include autoconf, override all previous options */
		rc = od_cfg_try_autoconf(path, model, diags);
		if (rc != 0) {
			return -1;
		}
		return od_cfg_validate_model(model, diags);
	}

	return 0;
}

int od_cfg_parse_file(const char *path, od_cfg_model_t *model,
		      od_cfg_diag_list_t *diags)
{
	return od_cfg_parse_file_depth(path, model, diags, 0, 1);
}

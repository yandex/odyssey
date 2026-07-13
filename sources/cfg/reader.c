/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <cfg/scan.h>
#include <cfg/model.h>
#include <cfg/ctx.h>
#include <cfg/reader.h>

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

int od_cfg_parse_file(const char *path, od_cfg_model_t *model,
		      od_cfg_diag_list_t *diags)
{
	char *buf = NULL;
	size_t size = 0;

	int rc = read_file(path, &buf, &size, diags);
	if (rc != 0) {
		return -1;
	}

	od_cfg_parse_ctx_t ctx;
	od_cfg_parse_ctx_init(&ctx, path, buf, size, model, diags);

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

	return od_cfg_validate_model(model, diags);
}

#include <stdarg.h>

#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

typedef struct {
	kiwi_var_type_t t;
	const char *v;
} varinfo_t;

static inline varinfo_t var(kiwi_var_type_t type, const char *value)
{
	varinfo_t vi;

	vi.t = type;
	vi.v = value;

	return vi;
}

static inline void do_pgoptions_test(const char *str, int expected_rc,
				     int count, ...)
{
	va_list ap;
	va_start(ap, count);

	kiwi_vars_t expected_vars;
	kiwi_vars_init(&expected_vars);

	for (int i = 0; i < count; ++i) {
		varinfo_t vi = va_arg(ap, varinfo_t);
		test(kiwi_vars_set(&expected_vars, vi.t, vi.v,
				   strlen(vi.v) + 1) == 0);
	}

	kiwi_vars_t vars;
	kiwi_vars_init(&vars);
	int rc = kiwi_parse_options_and_update_vars(
		&vars, str, str != NULL ? strlen(str) : 0);
	test(rc == expected_rc);
	if (rc != 0) {
		/* check error */
	} else {
		for (kiwi_var_type_t type = KIWI_VAR_CLIENT_ENCODING;
		     type < KIWI_VAR_MAX; ++type) {
			kiwi_var_t *expected_var =
				kiwi_vars_of(&expected_vars, type);
			kiwi_var_t *var = kiwi_vars_of(&vars, type);

			test(var->name_len == expected_var->name_len);
			test(strncasecmp(var->name, expected_var->name,
					 var->name_len) == 0);
			test(var->value_len == expected_var->value_len);
			test(strncmp(var->value, expected_var->value,
				     var->value_len) == 0);
		}
	}

	va_end(ap);
}

void kiwi_test_pgoptions(void)
{
	do_pgoptions_test("-c statement_timeout=19", 0 /* expected_rc */,
			  1, /* vars count */
			  var(KIWI_VAR_STATEMENT_TIMEOUT, "19"));
	do_pgoptions_test("    -c statement_timeout=19", 0 /* expected_rc */,
			  1, /* vars count */
			  var(KIWI_VAR_STATEMENT_TIMEOUT, "19"));
	do_pgoptions_test("-c      statement_timeout=19", 0 /* expected_rc */,
			  1, /* vars count */
			  var(KIWI_VAR_STATEMENT_TIMEOUT, "19"));
	do_pgoptions_test("-c      statement_timeout=", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_STATEMENT_TIMEOUT, ""));
	do_pgoptions_test("--search_path=public", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "public"));
	do_pgoptions_test("-c search_path=public", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "public"));
	do_pgoptions_test("-c search_path=\"$user\",\\ public",
			  0 /* expected_rc */, 1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "\"$user\", public"));
	do_pgoptions_test(
		"-c search_path=public --statement_timeout=1000 -c work_mem=4MB",
		0 /* expected_rc */, 3 /* vars count */,
		var(KIWI_VAR_SEARCH_PATH, "public"),
		var(KIWI_VAR_STATEMENT_TIMEOUT, "1000"),
		var(KIWI_VAR_WORK_MEM, "4MB"));
	do_pgoptions_test(
		"   -c     search_path=public      --statement_timeout=1000    -c    work_mem=4MB",
		0 /* expected_rc */, 3 /* vars count */,
		var(KIWI_VAR_SEARCH_PATH, "public"),
		var(KIWI_VAR_STATEMENT_TIMEOUT, "1000"),
		var(KIWI_VAR_WORK_MEM, "4MB"));
	do_pgoptions_test("-c search_path=my\\ schema", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "my schema"));
	do_pgoptions_test("-c search_path=path\\\\end", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "path\\end"));
	do_pgoptions_test("-c search_path=popatf\\", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "popatf"));
	do_pgoptions_test("-c search_path=popa\\tf", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "popatf"));
	do_pgoptions_test("-c search_path=popa\\tf", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "popatf"));
	do_pgoptions_test("-c search_path=popa\\tf", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "popatf"));
	do_pgoptions_test("--statement-timeout=1000", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_STATEMENT_TIMEOUT, "1000"));
	do_pgoptions_test("-c search_path=", 0 /* expected_rc */,
			  1 /* vars count */, var(KIWI_VAR_SEARCH_PATH, ""));
	do_pgoptions_test("-c search_path=val\\=ue", 0 /* expected_rc */,
			  1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "val=ue"));
	do_pgoptions_test("  -c  search_path=public   --statement_timeout=1000",
			  0 /* expected_rc */, 2 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH, "public"),
			  var(KIWI_VAR_STATEMENT_TIMEOUT, "1000"));
	do_pgoptions_test("-c search_path=my\\ schema\\\\with\\\\backslash",
			  0 /* expected_rc */, 1 /* vars count */,
			  var(KIWI_VAR_SEARCH_PATH,
			      "my schema\\with\\backslash"));
	do_pgoptions_test("", 0 /* expected_rc */, 0 /* vars count */);

	do_pgoptions_test(NULL, -1 /* expected_rc */, 0 /* vars count */);
	do_pgoptions_test("-c      statement_timeout", -1 /* expected_rc */,
			  0 /* vars count */);
	do_pgoptions_test("-c statement_timeout=     19", -1 /* expected_rc */,
			  0 /* vars count */);
	do_pgoptions_test("-c statement_timeout   =19", -1 /* expected_rc */,
			  0 /* vars count */);
	do_pgoptions_test("-c statement_timeout    =    19",
			  -1 /* expected_rc */, 0 /* vars count */);
	do_pgoptions_test("  -c     statement_timeout = 19   ",
			  -1 /* expected_rc */, 0 /* vars count */);
	do_pgoptions_test("-c      statement_time=1337", -1 /* expected_rc */,
			  0 /* vars count */);
	do_pgoptions_test("-c search_path", -1 /* expected_rc */,
			  0 /* vars count */);
	do_pgoptions_test("-c -c search_path", -1 /* expected_rc */,
			  0 /* vars count */);
	do_pgoptions_test("-x search_path=public", -1 /* expected_rc */,
			  0 /* vars count */);

	{
		char opts[512];
		char value[256];

		memset(value, 'a', 200);
		value[200] = 0;
		snprintf(opts, sizeof(opts), "--search_path=%s", value);
		do_pgoptions_test(opts, -1 /* expected_rc */,
				  0 /* vars count */);

		memset(value, 'a', 200);
		value[200] = 0;
		snprintf(opts, sizeof(opts), "--%s=x", value);
		do_pgoptions_test(opts, -1 /* expected_rc */,
				  0 /* vars count */);

		/* max allowed value is KIWI_MAX_VAR_SIZE - 1 chars */
		memset(value, 'a', KIWI_MAX_VAR_SIZE - 1);
		value[KIWI_MAX_VAR_SIZE - 1] = 0;
		snprintf(opts, sizeof(opts), "--search_path=%s", value);
		do_pgoptions_test(opts, 0 /* expected_rc */, 1 /* vars count */,
				  var(KIWI_VAR_SEARCH_PATH, value));
	}
}

static void build_startup_packet(char *buf, size_t cap, size_t *out_len,
				 const char *options_value)
{
	size_t pos = 0;

	/* length placeholder */
	buf[pos++] = 0;
	buf[pos++] = 0;
	buf[pos++] = 0;
	buf[pos++] = 0;

	/* protocol version 3.0 */
	buf[pos++] = 0;
	buf[pos++] = 3;
	buf[pos++] = 0;
	buf[pos++] = 0;

	memcpy(buf + pos, "user", 5);
	pos += 5;
	memcpy(buf + pos, "test", 5);
	pos += 5;

	memcpy(buf + pos, "options", 8);
	pos += 8;

	size_t value_len = strlen(options_value) + 1;
	test(cap - pos > value_len + 1);
	memcpy(buf + pos, options_value, value_len);
	pos += value_len;

	buf[pos++] = '\0';

	uint32_t len = pos;
	buf[0] = (len >> 24) & 0xFF;
	buf[1] = (len >> 16) & 0xFF;
	buf[2] = (len >> 8) & 0xFF;
	buf[3] = len & 0xFF;

	*out_len = pos;
}

void kiwi_test_be_read_startup_options(void)
{
	char buf[1024];
	size_t len;
	kiwi_be_startup_t su;
	kiwi_vars_t vars;

	/* valid options are applied */
	build_startup_packet(buf, sizeof(buf), &len, "--search_path=public");
	kiwi_be_startup_init(&su);
	kiwi_vars_init(&vars);
	test(kiwi_be_read_startup(buf, len, &su, &vars) ==
	     KIWI_STARTUP_READ_OK);
	kiwi_var_t *var = kiwi_vars_get(&vars, KIWI_VAR_SEARCH_PATH);
	test(var != NULL);
	test(var->value_len == (int)(strlen("public") + 1));
	test(strcmp(var->value, "public") == 0);

	/* too long option value must reject the whole startup packet */
	{
		char value[256];
		memset(value, 'a', 200);
		value[200] = 0;
		char opts[512];
		snprintf(opts, sizeof(opts), "--search_path=%s", value);
		build_startup_packet(buf, sizeof(buf), &len, opts);
		kiwi_be_startup_init(&su);
		kiwi_vars_init(&vars);
		test(kiwi_be_read_startup(buf, len, &su, &vars) ==
		     KIWI_STARTUP_READ_OPTIONS_ERROR);
	}

	/* unexpected token must reject the whole startup packet */
	build_startup_packet(buf, sizeof(buf), &len, "-x search_path=public");
	kiwi_be_startup_init(&su);
	kiwi_vars_init(&vars);
	test(kiwi_be_read_startup(buf, len, &su, &vars) ==
	     KIWI_STARTUP_READ_OPTIONS_ERROR);
}

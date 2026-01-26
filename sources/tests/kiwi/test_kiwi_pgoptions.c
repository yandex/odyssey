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
}

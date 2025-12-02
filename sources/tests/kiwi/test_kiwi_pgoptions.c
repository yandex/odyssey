#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

void test_pgoptions_simple()
{
	kiwi_vars_t kv;
	kiwi_var_t *var;
	uint32_t testcnt;
	uint32_t tt;

	kiwi_vars_init(&kv);

	char *teststrs[] = {
		"-c statement_timeout=19",
		"-c statement_timeout=     19",
		"-c statement_timeout   =19",
		"-c statement_timeout    =    19",
		"    -c statement_timeout=19",
		"-c      statement_timeout=19",
		"  -c     statement_timeout = 19   ",
	};

	testcnt = sizeof(teststrs) / sizeof(char *);

	for (tt = 0; tt < testcnt; ++tt) {
		kiwi_parse_options_and_update_vars(&kv, teststrs[tt],
						   strlen(teststrs[tt]));

		var = kiwi_vars_get(&kv, KIWI_VAR_STATEMENT_TIMEOUT);
		test(var != NULL);
		test(strncmp(var->name, "statement_timeout", var->name_len) ==
		     0);
		test(strncmp(var->value, "19", var->value_len) == 0);
	}
}

void test_pgoptions_fail()
{
	kiwi_vars_t kv;
	kiwi_var_t *var;

	kiwi_vars_init(&kv);

	char *teststr = "-c statement_time=19";

	kiwi_parse_options_and_update_vars(&kv, teststr, strlen(teststr));

	var = kiwi_vars_get(&kv, KIWI_VAR_STATEMENT_TIMEOUT);
	test(var == NULL);
}

void kiwi_test_pgoptions()
{
	test_pgoptions_simple();
	test_pgoptions_fail();
}

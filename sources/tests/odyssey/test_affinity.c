#include <odyssey.h>

#include <affinity.h>
#include <util.h>
#include <tests/odyssey_test.h>

static void do_cpuset_parse_test(const char *str, int n, ...)
{
	va_list args;
	va_start(args, n);

	od_affinity_cpuset_t set;
	int rc = od_affinity_cpuset_parse(str, &set, NULL, 0);
	if (n == 0) {
		test(rc < 0);
	} else {
		test(rc == 0);
		test(n == od_affinity_cpuset_count(&set));
		for (int i = 0; i < n; ++i) {
			int num = va_arg(args, int);
			test(od_affinity_cpuset_get(&set, num) == 1);
		}
	}

	va_end(args);
}

static void cpuset_parse_tests(void)
{
	do_cpuset_parse_test("", 0);
	do_cpuset_parse_test(",", 0);
	do_cpuset_parse_test(",,", 0);
	do_cpuset_parse_test(",0", 0);
	do_cpuset_parse_test("0,", 0);
	do_cpuset_parse_test("0,,1", 0);

	do_cpuset_parse_test("-", 0);
	do_cpuset_parse_test("0-", 0);
	do_cpuset_parse_test("-1", 0);
	do_cpuset_parse_test("1-", 0);
	do_cpuset_parse_test("1--2", 0);

	do_cpuset_parse_test("a", 0);
	do_cpuset_parse_test("abc", 0);
	do_cpuset_parse_test("a1", 0);
	do_cpuset_parse_test("1a", 0);
	do_cpuset_parse_test("1,a", 0);
	do_cpuset_parse_test("a,1", 0);
	do_cpuset_parse_test("1-2x", 0);
	do_cpuset_parse_test("1x-2", 0);

	do_cpuset_parse_test("2-1", 0);
	do_cpuset_parse_test("10-3", 0);

	do_cpuset_parse_test("1 2", 0);
	do_cpuset_parse_test("1 -2", 0);
	do_cpuset_parse_test("1- 2", 0);
	do_cpuset_parse_test("1 ,2", 0);
	do_cpuset_parse_test("1, 2", 0);

	do_cpuset_parse_test("0", 1, 0);
	do_cpuset_parse_test("1", 1, 1);
	do_cpuset_parse_test("7", 1, 7);

	do_cpuset_parse_test("0,1", 2, 0, 1);
	do_cpuset_parse_test("1,0", 2, 0, 1);
	do_cpuset_parse_test("0,1,2", 3, 0, 1, 2);
	do_cpuset_parse_test("2,4,6", 3, 2, 4, 6);

	do_cpuset_parse_test("0-0", 1, 0);
	do_cpuset_parse_test("0-1", 2, 0, 1);
	do_cpuset_parse_test("1-3", 3, 1, 2, 3);
	do_cpuset_parse_test("3-5", 3, 3, 4, 5);

	do_cpuset_parse_test("0-2,4", 4, 0, 1, 2, 4);
	do_cpuset_parse_test("0,2-4", 4, 0, 2, 3, 4);
	do_cpuset_parse_test("0-2,4-6", 6, 0, 1, 2, 4, 5, 6);
	do_cpuset_parse_test("1,3-5,7", 5, 1, 3, 4, 5, 7);
	do_cpuset_parse_test("0,2,4-6,8", 6, 0, 2, 4, 5, 6, 8);

	do_cpuset_parse_test("0,0", 1, 0);
	do_cpuset_parse_test("1,1,1", 1, 1);
	do_cpuset_parse_test("0-2,1", 3, 0, 1, 2);
	do_cpuset_parse_test("1,0-2", 3, 0, 1, 2);
	do_cpuset_parse_test("0-2,1-3", 4, 0, 1, 2, 3);
	do_cpuset_parse_test("1,2,2,3,3,3", 3, 1, 2, 3);

	do_cpuset_parse_test("0", 1, 0);
	do_cpuset_parse_test("0-1", 2, 0, 1);

	{
		char buf[64];
		od_snprintf(buf, sizeof(buf), "%d", OD_AFFINITY_MAX_CPUS - 1);
		do_cpuset_parse_test(buf, 1, OD_AFFINITY_MAX_CPUS - 1);
	}
	{
		char buf[64];
		od_snprintf(buf, sizeof(buf), "%d-%d", OD_AFFINITY_MAX_CPUS - 2,
			    OD_AFFINITY_MAX_CPUS - 1);
		do_cpuset_parse_test(buf, 2, OD_AFFINITY_MAX_CPUS - 2,
				     OD_AFFINITY_MAX_CPUS - 1);
	}

	{
		char buf[64];
		od_snprintf(buf, sizeof(buf), "%d", OD_AFFINITY_MAX_CPUS);
		do_cpuset_parse_test(buf, 0);
	}
	{
		char buf[64];
		od_snprintf(buf, sizeof(buf), "0-%d", OD_AFFINITY_MAX_CPUS);
		do_cpuset_parse_test(buf, 0);
	}
	{
		char buf[64];
		od_snprintf(buf, sizeof(buf), "%d-%d", OD_AFFINITY_MAX_CPUS - 1,
			    OD_AFFINITY_MAX_CPUS);
		do_cpuset_parse_test(buf, 0);
	}

	do_cpuset_parse_test("-0", 0);
	do_cpuset_parse_test("-1", 0);
	do_cpuset_parse_test("0,-1", 0);
	do_cpuset_parse_test("-1,0", 0);
	do_cpuset_parse_test("-1--2", 0);

	do_cpuset_parse_test("0x", 0);
	do_cpuset_parse_test("1-2x", 0);
	do_cpuset_parse_test("1,2x", 0);
	do_cpuset_parse_test("1-2,3x", 0);
	do_cpuset_parse_test("1-2,", 0);

	do_cpuset_parse_test("5,3,1", 3, 1, 3, 5);
	do_cpuset_parse_test("5-7,1-3", 6, 1, 2, 3, 5, 6, 7);
	do_cpuset_parse_test("1,3-4,6,8-9", 6, 1, 3, 4, 6, 8, 9);
}

static void do_rule_parse_test(const char *str, od_affinity_role_t role,
			       int index, int n, ...)
{
	od_affinity_rule_t rule;
	od_affinity_rule_init(&rule);

	va_list args;
	va_start(args, n);

	int rc = od_affinity_rule_parse(str, &rule, NULL, 0);
	if (n == 0) {
		test(rc < 0);
	} else {
		test(rc == 0);
		test(rule.index == index);
		test(rule.role == role);
		test(n == od_affinity_cpuset_count(&rule.cpuset));
		for (int i = 0; i < n; ++i) {
			int num = va_arg(args, int);
			test(od_affinity_cpuset_get(&rule.cpuset, num) == 1);
		}
	}

	va_end(args);
}

static void rule_parse_tests(void)
{
	do_rule_parse_test("", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test(":", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker:", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker:3-4handshake", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake:", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test(":0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test(":0-3", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("wrk:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("workers:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("hs:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("foo:0-3", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("worker[]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker[:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker[0:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker0]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker[0]]:0", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("handshake[]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake[:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake[0:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake0]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake[0]]:0", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("worker[-1]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker[abc]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker[1x]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker[x1]:0", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("handshake[-1]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake[abc]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake[1x]:0", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake[x1]:0", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("worker:abc", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker:1,", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker:,1", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("worker:3-1", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("handshake:abc", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake:1,", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake:,1", OD_AFFINITY_ROLE_NONE, -1, 0);
	do_rule_parse_test("handshake:3-1", OD_AFFINITY_ROLE_NONE, -1, 0);

	do_rule_parse_test("worker:0", OD_AFFINITY_ROLE_WORKER, -1, 1, 0);
	do_rule_parse_test("worker:1", OD_AFFINITY_ROLE_WORKER, -1, 1, 1);
	do_rule_parse_test("worker:0-3", OD_AFFINITY_ROLE_WORKER, -1, 4, 0, 1,
			   2, 3);
	do_rule_parse_test("worker:1,3,5", OD_AFFINITY_ROLE_WORKER, -1, 3, 1, 3,
			   5);
	do_rule_parse_test("worker:0-2,4", OD_AFFINITY_ROLE_WORKER, -1, 4, 0, 1,
			   2, 4);

	do_rule_parse_test("handshake:0", OD_AFFINITY_ROLE_HANDSHAKE, -1, 1, 0);
	do_rule_parse_test("handshake:2", OD_AFFINITY_ROLE_HANDSHAKE, -1, 1, 2);
	do_rule_parse_test("handshake:2-3", OD_AFFINITY_ROLE_HANDSHAKE, -1, 2,
			   2, 3);
	do_rule_parse_test("handshake:1,4,7", OD_AFFINITY_ROLE_HANDSHAKE, -1, 3,
			   1, 4, 7);
	do_rule_parse_test("handshake:0-1,3-4", OD_AFFINITY_ROLE_HANDSHAKE, -1,
			   4, 0, 1, 3, 4);

	do_rule_parse_test("worker[0]:0", OD_AFFINITY_ROLE_WORKER, 0, 1, 0);
	do_rule_parse_test("worker[1]:1", OD_AFFINITY_ROLE_WORKER, 1, 1, 1);
	do_rule_parse_test("worker[7]:2-3", OD_AFFINITY_ROLE_WORKER, 7, 2, 2,
			   3);
	do_rule_parse_test("worker[10]:0,2,4", OD_AFFINITY_ROLE_WORKER, 10, 3,
			   0, 2, 4);

	do_rule_parse_test("handshake[0]:0", OD_AFFINITY_ROLE_HANDSHAKE, 0, 1,
			   0);
	do_rule_parse_test("handshake[1]:3", OD_AFFINITY_ROLE_HANDSHAKE, 1, 1,
			   3);
	do_rule_parse_test("handshake[2]:4-5", OD_AFFINITY_ROLE_HANDSHAKE, 2, 2,
			   4, 5);
	do_rule_parse_test("handshake[9]:1,3,7", OD_AFFINITY_ROLE_HANDSHAKE, 9,
			   3, 1, 3, 7);

	do_rule_parse_test("worker:0,0", OD_AFFINITY_ROLE_WORKER, -1, 1, 0);
	do_rule_parse_test("worker:0-2,1", OD_AFFINITY_ROLE_WORKER, -1, 3, 0, 1,
			   2);
	do_rule_parse_test("handshake:2,2,2", OD_AFFINITY_ROLE_HANDSHAKE, -1, 1,
			   2);
	do_rule_parse_test("handshake:1-3,2-4", OD_AFFINITY_ROLE_HANDSHAKE, -1,
			   4, 1, 2, 3, 4);
}

static od_affinity_rule_t rule(od_affinity_role_t role, int index, int n, ...)
{
	od_affinity_rule_t rule;
	od_affinity_rule_init(&rule);

	va_list args;
	va_start(args, n);

	rule.role = role;
	rule.index = index;
	for (int i = 0; i < n; ++i) {
		int num = va_arg(args, int);
		od_affinity_cpuset_add(&rule.cpuset, num);
	}

	va_end(args);

	return rule;
}

static void do_config_parse_test(const char *str, int rc,
				 od_affinity_mode_t mode, size_t n, ...)
{
	od_affinity_config_t config;

	int sut = od_affinity_config_parse(str, strlen(str), &config, NULL, 0);
	test(sut == rc);

	if (rc < 0) {
		return;
	}

	test(config.mode == mode);
	if (mode == OD_AFFINITY_MODE_AUTO || mode == OD_AFFINITY_MODE_OFF) {
		test(config.nrules == 0);
	} else {
		test(config.nrules == n);

		va_list args;
		va_start(args, n);

		for (size_t i = 0; i < n; ++i) {
			od_affinity_rule_t expected =
				va_arg(args, od_affinity_rule_t);
			test(memcmp(&config.rules[i], &expected,
				    sizeof(od_affinity_rule_t)) == 0);
		}

		va_end(args);
	}
}

static void config_parse_tests(void)
{
	do_config_parse_test("", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test(" ", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("\t", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("   \t   ", -1, OD_AFFINITY_MODE_OFF, 0);

	do_config_parse_test("off", 0, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("no", 0, OD_AFFINITY_MODE_OFF, 0);

	do_config_parse_test("auto", 0, OD_AFFINITY_MODE_AUTO, 0);
	do_config_parse_test("yes", 0, OD_AFFINITY_MODE_AUTO, 0);

	do_config_parse_test("off worker:0", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("auto worker:0", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("yes worker:0", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("no worker:0", -1, OD_AFFINITY_MODE_OFF, 0);

	do_config_parse_test("worker:0", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_WORKER, -1, 1, 0));

	do_config_parse_test("worker:0-4,6", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_WORKER, -1, 6, 0, 1, 2, 3, 4,
				  6));

	do_config_parse_test("handshake:1", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 1, 1));

	do_config_parse_test("handshake:2-3", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 2, 2, 3));

	do_config_parse_test("worker[0]:0", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_WORKER, 0, 1, 0));

	do_config_parse_test("worker[7]:2-3", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_WORKER, 7, 2, 2, 3));

	do_config_parse_test("handshake[0]:4", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, 0, 1, 4));

	do_config_parse_test("handshake[2]:4-5", 0, OD_AFFINITY_MODE_RULES, 1,
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, 2, 2, 4, 5));

	do_config_parse_test("worker:0-3 handshake:4-5", 0,
			     OD_AFFINITY_MODE_RULES, 2,
			     rule(OD_AFFINITY_ROLE_WORKER, -1, 4, 0, 1, 2, 3),
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 2, 4, 5));

	do_config_parse_test("worker[0]:0 worker[1]:1 handshake:2-3", 0,
			     OD_AFFINITY_MODE_RULES, 3,
			     rule(OD_AFFINITY_ROLE_WORKER, 0, 1, 0),
			     rule(OD_AFFINITY_ROLE_WORKER, 1, 1, 1),
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 2, 2, 3));

	do_config_parse_test("worker:4-7 handshake[0]:2 handshake[1]:3", 0,
			     OD_AFFINITY_MODE_RULES, 3,
			     rule(OD_AFFINITY_ROLE_WORKER, -1, 4, 4, 5, 6, 7),
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, 0, 1, 2),
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, 1, 1, 3));

	do_config_parse_test("worker:0-1\t\thandshake:2-3", 0,
			     OD_AFFINITY_MODE_RULES, 2,
			     rule(OD_AFFINITY_ROLE_WORKER, -1, 2, 0, 1),
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 2, 2, 3));

	do_config_parse_test("worker:0-1   handshake:2-3", 0,
			     OD_AFFINITY_MODE_RULES, 2,
			     rule(OD_AFFINITY_ROLE_WORKER, -1, 2, 0, 1),
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 2, 2, 3));

	do_config_parse_test("worker:0,0,1 handshake:2,2", 0,
			     OD_AFFINITY_MODE_RULES, 2,
			     rule(OD_AFFINITY_ROLE_WORKER, -1, 2, 0, 1),
			     rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 1, 2));

	do_config_parse_test("worker:0 worker:1", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake:0 handshake:1", -1,
			     OD_AFFINITY_MODE_OFF, 0);

	do_config_parse_test("worker[0]:0 worker[0]:1", -1,
			     OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake[1]:2 handshake[1]:3", -1,
			     OD_AFFINITY_MODE_OFF, 0);

	do_config_parse_test(
		"worker:0-3 worker[0]:4 handshake:5-6 handshake[1]:7", 0,
		OD_AFFINITY_MODE_RULES, 4,
		rule(OD_AFFINITY_ROLE_WORKER, -1, 4, 0, 1, 2, 3),
		rule(OD_AFFINITY_ROLE_WORKER, 0, 1, 4),
		rule(OD_AFFINITY_ROLE_HANDSHAKE, -1, 2, 5, 6),
		rule(OD_AFFINITY_ROLE_HANDSHAKE, 1, 1, 7));

	do_config_parse_test("worker", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("worker:", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake:", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("worker[]:0", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake[]:0", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("worker[abc]:0", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake[-1]:0", -1, OD_AFFINITY_MODE_OFF, 0);

	do_config_parse_test("worker:abc", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake:3-1", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("worker:1,", -1, OD_AFFINITY_MODE_OFF, 0);
	do_config_parse_test("handshake:,1", -1, OD_AFFINITY_MODE_OFF, 0);
}

void odyssey_test_affinity(void)
{
	cpuset_parse_tests();
	rule_parse_tests();
	config_parse_tests();
}

/*
 * Shared test data for regex benchmarks.
 * Same queries and pattern used across all three engines.
 */

#ifndef BENCH_DATA_H
#define BENCH_DATA_H

#include <string.h>

#define ITERATIONS 1000000
#define WARMUP 10000

/* Test queries: mix of legitimate and malicious */
static const char *queries[] = {
	/* legitimate */
	"SELECT id, name, email FROM users WHERE id = 42",
	"INSERT INTO orders (user_id, total) VALUES (1, 99.99)",
	"UPDATE accounts SET balance = balance - 10 WHERE id = 5",
	"SELECT count(*) FROM pg_class WHERE relkind = 'r'",
	"SELECT p.name, o.total FROM products p JOIN orders o ON p.id = o.product_id",
	"BEGIN; UPDATE accounts SET balance = 100 WHERE id = 1; COMMIT",
	"SELECT * FROM events WHERE created_at > now() - interval '1 hour'",
	"SELECT length('hello world')",
	"WITH cte AS (SELECT id FROM users) SELECT * FROM cte",
	"SELECT array_agg(name) FROM departments GROUP BY division",

	/* malicious */
	"SELECT * FROM users WHERE name = '' OR '1'='1'",
	"SELECT 1 UNION SELECT usename FROM pg_user",
	"DROP TABLE users",
	"SELECT pg_read_file('/etc/passwd', 0, 200)",
	"COPY shell FROM PROGRAM 'cat /etc/passwd'",
	"SELECT 1 FROM pg_sleep(10)",
	"SELECT usename, passwd FROM pg_shadow",
	"SELECT query_to_xml('select * from pg_user', true, true, '')",
	"CREATE OR REPLACE FUNCTION system(cstring) RETURNS int AS '/lib/x86_64-linux-gnu/libc.so.6', 'system' LANGUAGE 'c' STRICT",
	"'; DROP TABLE users;--",
};

static const int num_queries = sizeof(queries) / sizeof(queries[0]);

/* The combined pattern (same as what odyssey would compile from multiple
   sql_guard_regex lines) */
static const char *pattern =
	"(\\b(DROP|TRUNCATE)\\s+(TABLE|DATABASE|SCHEMA)\\b)"
	"|(\\bUNION\\s+(ALL\\s+)?SELECT\\b)"
	"|(\\bCOPY\\s+\\w+\\s+(FROM|TO)\\s+PROGRAM\\b)"
	"|(\\bCOPY\\s*\\([^)]*\\)\\s*TO\\s+PROGRAM\\b)"
	"|(\\bpg_sleep\\s*\\()"
	"|(\\bpg_read_file\\s*\\()"
	"|(\\blo_import\\s*\\()"
	"|(\\blo_export\\s*\\()"
	"|(\\blo_from_bytea\\s*\\()"
	"|(\\bCREATE\\s+(OR\\s+REPLACE\\s+)?FUNCTION\\b.*\\bLANGUAGE\\s+'c')"
	"|(\\binformation_schema\\.(tables|columns|role_table_grants)\\b)"
	"|(\\bpg_shadow\\b)"
	"|(\\bdatabase_to_xml\\s*\\()"
	"|(\\bquery_to_xml\\s*\\()"
	"|(\\b(OR|AND)\\s+['0-9].*=\\s*['0-9])"
	"|(';\\s*(DROP|DELETE|INSERT|UPDATE|CREATE)\\b)";

static double time_diff_ms(struct timespec *start, struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) * 1000.0 +
	       (end->tv_nsec - start->tv_nsec) / 1000000.0;
}

#endif /* BENCH_DATA_H */

/*
 * Benchmark: sql_guard 5 modes
 *
 * 1. sql_guard disabled       — no check at all (baseline)
 * 2. blacklist (no cache)     — POSIX ERE regex check every query
 * 3. blacklist + cache        — hash cache skips regex on repeated queries
 * 4. whitelist (no cache)     — regex check, invert result
 * 5. whitelist + cache        — hash cache with inverted result
 *
 * Uses murmur hash (same as odyssey codebase od_murmur_hash).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <regex.h>

#include "bench_data.h"

/* --- murmur hash (same as odyssey/sources/murmurhash.c) --- */

static uint32_t murmur_hash(const void *raw, size_t len)
{
	const unsigned int m = 0xc6a4a793;
	const int r = 16;
	unsigned int h = 0x5bd1e995 ^ (len * m);
	unsigned int k;
	const unsigned char *data = (const unsigned char *)raw;
	char buf[4];
	memcpy(buf, data, len >= 4 ? 4 : len);

	while (len >= 4) {
		k = *(unsigned int *)buf;
		h += k;
		h *= m;
		h ^= h >> 16;
		data += 4;
		len -= 4;
		memcpy(buf, data, len >= 4 ? 4 : len);
	}

	switch (len) {
	case 3:
		h += buf[2] << 16;
		break;
	case 2:
		h += buf[1] << 8;
		break;
	case 1:
		h += buf[0];
		h *= m;
		h ^= h >> r;
		break;
	}

	h *= m;
	h ^= h >> 10;
	h *= m;
	h ^= h >> 17;
	return h;
}

/* --- Direct-mapped hash cache (same as odyssey implementation) --- */

#define CACHE_BITS 12
#define CACHE_SIZE (1 << CACHE_BITS)
#define CACHE_MASK (CACHE_SIZE - 1)

struct cache_entry {
	uint32_t hash;
	int result;
	int valid;
};

static struct cache_entry cache[CACHE_SIZE];

/* --- Mode 1: sql_guard disabled (baseline) --- */

static void bench_disabled(void)
{
	struct timespec t0, t1;

	printf("=== Mode 1: sql_guard disabled ===\n");

	/* warmup */
	volatile int sink = 0;
	for (int i = 0; i < WARMUP; i++) {
		sink += (int)strlen(queries[i % num_queries]);
	}

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (int i = 0; i < ITERATIONS; i++) {
		sink += (int)strlen(queries[i % num_queries]);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);

	double ms = time_diff_ms(&t0, &t1);
	printf("  %d iterations: %.1f ms (%.0f ops/sec)\n", ITERATIONS, ms,
	       ITERATIONS / (ms / 1000.0));
	printf("  avg per op: %.3f us\n", (ms * 1000.0) / ITERATIONS);
	printf("  (no regex check — just query forwarding overhead)\n\n");
	(void)sink;
}

/* --- Generic regex bench (no cache) --- */

static void bench_regex(regex_t *re, const char *label, int whitelist)
{
	struct timespec t0, t1;

	printf("=== %s ===\n", label);

	/* warmup */
	for (int i = 0; i < WARMUP; i++) {
		regexec(re, queries[i % num_queries], 0, NULL, 0);
	}

	int blocked = 0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (int i = 0; i < ITERATIONS; i++) {
		int matched =
			(regexec(re, queries[i % num_queries], 0, NULL, 0) ==
			 0) ?
				1 :
				0;
		if (whitelist ? !matched : matched)
			blocked++;
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);

	double ms = time_diff_ms(&t0, &t1);
	printf("  %d iterations: %.1f ms (%.0f ops/sec)\n", ITERATIONS, ms,
	       ITERATIONS / (ms / 1000.0));
	printf("  blocked: %d / %d\n", blocked, ITERATIONS);
	printf("  avg per op: %.3f us\n\n", (ms * 1000.0) / ITERATIONS);
}

/* --- Generic regex bench (with cache) --- */

static void bench_regex_cache(regex_t *re, const char *label, int whitelist)
{
	struct timespec t0, t1;

	printf("=== %s ===\n", label);

	/* clear cache */
	memset(cache, 0, sizeof(cache));

	/* warmup (populates cache) */
	for (int i = 0; i < WARMUP; i++) {
		const char *q = queries[i % num_queries];
		size_t qlen = strlen(q);
		uint32_t h = murmur_hash(q, qlen);
		uint32_t idx = h & CACHE_MASK;
		if (!(cache[idx].valid && cache[idx].hash == h)) {
			int matched =
				(regexec(re, q, 0, NULL, 0) == 0) ? 1 : 0;
			int res = whitelist ? !matched : matched;
			cache[idx].hash = h;
			cache[idx].result = res;
			cache[idx].valid = 1;
		}
	}

	/* clear cache for clean measurement */
	memset(cache, 0, sizeof(cache));

	int blocked = 0;
	int hits = 0, misses = 0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (int i = 0; i < ITERATIONS; i++) {
		const char *q = queries[i % num_queries];
		size_t qlen = strlen(q);
		uint32_t h = murmur_hash(q, qlen);
		uint32_t idx = h & CACHE_MASK;
		int res;
		if (cache[idx].valid && cache[idx].hash == h) {
			res = cache[idx].result;
			hits++;
		} else {
			int matched =
				(regexec(re, q, 0, NULL, 0) == 0) ? 1 : 0;
			res = whitelist ? !matched : matched;
			cache[idx].hash = h;
			cache[idx].result = res;
			cache[idx].valid = 1;
			misses++;
		}
		if (res)
			blocked++;
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);

	double ms = time_diff_ms(&t0, &t1);
	printf("  %d iterations: %.1f ms (%.0f ops/sec)\n", ITERATIONS, ms,
	       ITERATIONS / (ms / 1000.0));
	printf("  blocked: %d / %d\n", blocked, ITERATIONS);
	printf("  cache hits: %d (%.1f%%), misses: %d\n", hits,
	       100.0 * hits / ITERATIONS, misses);
	printf("  avg per op: %.3f us\n\n", (ms * 1000.0) / ITERATIONS);
}

/* --- Main --- */

int main(void)
{
	regex_t re;

	printf("sql_guard benchmark: 5 modes\n");
	printf("Pattern length: %zu chars\n", strlen(pattern));
	printf("Queries: %d (10 legitimate, 10 malicious)\n", num_queries);
	printf("Iterations: %d per mode\n", ITERATIONS);
	printf("Cache: direct-mapped, %d entries, murmur hash\n\n", CACHE_SIZE);

	int rc = regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB | REG_ICASE);
	if (rc != 0) {
		char errbuf[256];
		regerror(rc, &re, errbuf, sizeof(errbuf));
		printf("compile FAILED: %s\n", errbuf);
		return 1;
	}
	printf("regex compile: OK\n\n");

	bench_disabled();
	bench_regex(&re, "Mode 2: blacklist (no cache)", 0);
	bench_regex_cache(&re, "Mode 3: blacklist + cache", 0);
	bench_regex(&re, "Mode 4: whitelist (no cache)", 1);
	bench_regex_cache(&re, "Mode 5: whitelist + cache", 1);

	regfree(&re);
	return 0;
}

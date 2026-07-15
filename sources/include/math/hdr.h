#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Yet another implementation of historams, this one is for simple cases,
 * when accuracy is not needed
 */

#include <stdint.h>
#include <stddef.h>

#define OD_SIMPLE_HDR_MAX_BUCKETS 64

typedef struct {
	uint64_t bounds[OD_SIMPLE_HDR_MAX_BUCKETS];
	uint64_t counts[OD_SIMPLE_HDR_MAX_BUCKETS + 1];
	size_t nbounds;
	uint64_t total_count;
} od_simple_hdr_t;

void od_simple_hdr_init_log(od_simple_hdr_t *h, uint64_t min, uint64_t max,
			    double factor);
void od_simple_hdr_record(od_simple_hdr_t *h, uint64_t value);
uint64_t od_simple_hdr_percentile(od_simple_hdr_t *h, double perc);

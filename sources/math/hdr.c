#include <odyssey.h>

#include <math/hdr.h>

void od_simple_hdr_init_log(od_simple_hdr_t *h, uint64_t min, uint64_t max,
			    double factor)
{
	memset(h->counts, 0, sizeof(h->counts));
	memset(h->bounds, 0, sizeof(h->bounds));
	h->total_count = 0;

	int i;
	double v = (double)min;

	for (i = 0; v < max && i < OD_SIMPLE_HDR_MAX_BUCKETS - 1; ++i) {
		h->bounds[i] = (uint64_t)v;
		v *= factor;
	}

	h->bounds[i++] = max;
	h->nbounds = i;
}

void od_simple_hdr_record(od_simple_hdr_t *h, uint64_t value)
{
	++h->total_count;

	for (size_t i = 0; i < h->nbounds; ++i) {
		if (value <= h->bounds[i]) {
			++h->counts[i];
			return;
		}
	}

	++h->counts[h->nbounds];
}

uint64_t od_simple_hdr_percentile(od_simple_hdr_t *h, double perc)
{
	if (h->total_count == 0) {
		return 0;
	}

	uint64_t target = (uint64_t)ceil(h->total_count * perc / 100.0);
	uint64_t cumulative = 0;
	uint64_t prev_bound = 0;

	for (size_t i = 0; i <= h->nbounds; ++i) {
		uint64_t bucket_count = h->counts[i];
		uint64_t bucket_upper = (i < h->nbounds) ?
						h->bounds[i] :
						h->bounds[h->nbounds - 1] * 10;

		if (cumulative + bucket_count >= target) {
			if (bucket_count == 0) {
				return bucket_upper;
			}

			double dc =
				(double)(target - cumulative) / bucket_count;
			uint64_t interpolated =
				prev_bound +
				(uint64_t)(dc * (bucket_upper - prev_bound));

			return interpolated;
		}

		cumulative += bucket_count;
		prev_bound = bucket_upper;
	}

	return prev_bound;
}

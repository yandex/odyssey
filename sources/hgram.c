
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

void od_hgram_init(od_hgram_t *hgram)
{
	memset(hgram, 0, sizeof(*hgram));
}

void od_hgram_add_data_point(od_hgram_t *hgram, uint64_t point)
{
	uint64_t next_position = __sync_fetch_and_add(&hgram->estimated_size, 1);
	if (next_position < OD_HGRAM_DATA_POINTS) {
		hgram->data[next_position] = point;
		return;
	}
	next_position = machine_lrand48() % next_position;
	if (next_position < OD_HGRAM_DATA_POINTS)
		hgram->data[next_position] = point;
}

static int cmpfunc_ui32(const void *a, const void *b)
{
	if (*(uint32_t *) a < *(uint32_t *) b)
		return 1;
	if (*(uint32_t *) a > *(uint32_t *) b)
		return -1;
	return 0;
}

void od_hgram_freeze(od_hgram_t *hgram, od_hgram_frozen_t *hgram_tmp)
{
	memcpy(hgram_tmp, hgram, sizeof(*hgram));
	qsort(&hgram_tmp->data, OD_HGRAM_DATA_POINTS, sizeof(uint32_t), cmpfunc_ui32);

	if (hgram->estimated_size > OD_HGRAM_DATA_POINTS * 2) {
		hgram->estimated_size = hgram->estimated_size / 2;
		hgram_tmp->estimated_size = OD_HGRAM_DATA_POINTS;
	} else if (hgram->estimated_size > OD_HGRAM_DATA_POINTS) {
		hgram->estimated_size = OD_HGRAM_DATA_POINTS;
		hgram_tmp->estimated_size = OD_HGRAM_DATA_POINTS;
	}
}

uint64_t od_hgram_percentile(od_hgram_frozen_t *frozen, double percentile)
{
	assert(percentile > 0);
	assert(percentile <= 1);
	return frozen->data[(size_t)(frozen->estimated_size * (1 - percentile))];
}
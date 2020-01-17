#include <machinarium.h>
#include <odyssey_test.h>
#include <assert.h>
#include <lrand48.h>
#include "sources/hgram.h"

void hgram_forward_test();

void hgram_backward_test();

int hgram_random_test();

// Currently we are linking without lm
static int myround(double x)
{
	return x + 0.5;
}

void
machinarium_test_hgram(void)
{
	machinarium_init();
	mm_lrand48_seed();

	hgram_forward_test();
	hgram_backward_test();

	int fails = 0;
	for (int i=0;i<20;i++)
		fails += hgram_random_test();

	// fails approx 1/1000, so suppress flaps to impossible
	assert(fails <= 3);

	machinarium_free();
}

int hgram_random_test()
{
	od_hgram_t hgram;
	od_hgram_frozen_t f;
	od_hgram_init(&hgram);

	for (int i = 0; i < 100000; i++) {
		od_hgram_add_data_point(&hgram, machine_lrand48() % 10000);
	}

	od_hgram_freeze(&hgram, &f);

	int result = 0;
	if(myround(od_hgram_quantile(&f, 0.8) / 2000.0) != 4)
		result++;
	if(myround(od_hgram_quantile(&f, 0.6) / 2000.0) != 3)
		result++;
	if(myround(od_hgram_quantile(&f, 0.4) / 2000.0) != 2)
		result++;

	return result;
}

void hgram_backward_test()
{
	od_hgram_t hgram;
	od_hgram_frozen_t f;
	od_hgram_init(&hgram);

	for (int i = 0; i < 100; i++) {
		od_hgram_add_data_point(&hgram, 100 - i);
	}

	od_hgram_freeze(&hgram, &f);

	assert(od_hgram_quantile(&f, 0.7) == 70);
	assert(od_hgram_quantile(&f, 0.5) == 50);
	assert(od_hgram_quantile(&f, 0.3) == 30);
}

void hgram_forward_test()
{
	od_hgram_t hgram;
	od_hgram_frozen_t f;
	od_hgram_init(&hgram);

	for (int i = 1; i <= 100; i++) {
		od_hgram_add_data_point(&hgram, i);
	}
	od_hgram_freeze(&hgram, &f);

	assert(od_hgram_quantile(&f, 0.7) == 70);
	assert(od_hgram_quantile(&f, 0.5) == 50);
	assert(od_hgram_quantile(&f, 0.3) == 30);
}

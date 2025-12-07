
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <counter.h>
#include <od_memory.h>

static inline void check_value(od_counter_t *counter, size_t value)
{
	if (od_unlikely(value >= counter->size)) {
		/* no need any special action, it is an error about constant sizes */
		abort();
	}
}

od_counter_t *od_counter_create(size_t max_value)
{
	if (max_value > OD_COUNTER_MAX_POSSIBLE_VALUE) {
		/*
		 * counter should be used only for some
		 * small ranges, like enum for status
		 */
		abort();
	}

	/* 0..max_value */
	size_t values_count = max_value + 1;

	od_counter_t *counter = od_malloc(
		sizeof(od_counter_t) + sizeof(od_atomic_u64_t) * values_count);
	if (od_unlikely(counter == NULL)) {
		return NULL;
	}

	counter->size = values_count;

	for (size_t i = 0; i < counter->size; ++i) {
		od_atomic_u64_set(&counter->value_to_count[i], 0ULL);
	}

	return counter;
}

od_retcode_t od_counter_free(od_counter_t *counter)
{
	od_free(counter);

	return OK_RESPONSE;
}

void od_counter_inc(od_counter_t *counter, size_t value)
{
	check_value(counter, value);

	od_atomic_u64_inc(&counter->value_to_count[value]);
}

uint64_t od_counter_get_count(od_counter_t *counter, size_t value)
{
	check_value(counter, value);

	return od_atomic_u64_of(&counter->value_to_count[value]);
}

od_retcode_t od_counter_reset(od_counter_t *counter, size_t value)
{
	check_value(counter, value);

	od_atomic_u64_set(&counter->value_to_count[value], 0ULL);

	return OK_RESPONSE;
}

od_retcode_t od_counter_reset_all(od_counter_t *counter)
{
	for (size_t i = 0; i < counter->size; ++i) {
		od_atomic_u64_set(&counter->value_to_count[i], 0ULL);
	}

	return OK_RESPONSE;
}

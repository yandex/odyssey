/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

#if defined(__x86_64__) || defined(__i386) || defined(_X86_)
#define MM_MUTEX_BACKOFF __asm__("pause")
#else
#define MM_MUTEX_BACKOFF
#endif

#define MAX_SPIN_COUNT 30
#define NO_OWNER_ID (~0ULL)

/* exponential backoff, some kind... */
static const uint32_t sleep_durations_ms[] = {
	0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,   0,	0,   0,
	0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 100, 100, 100, 100,
};

static inline void check_machinarium_presence()
{
	if (mm_self == NULL || mm_self->scheduler.current == NULL) {
		/* do not use mm_mutex_t outside the machinarium */
		abort();
	}
}

static inline uint64_t get_current_coro_id()
{
	check_machinarium_presence();
	return mm_self->scheduler.current->id;
}

MACHINE_API void machine_mutex_init(machine_mutex_t *mutex)
{
	check_machinarium_presence();
	mutex->owner_coro_id = NO_OWNER_ID;
}

MACHINE_API void machine_mutex_destroy(machine_mutex_t *mutex)
{
	(void)mutex;
	assert(mm_atomic_u64_value(&mutex->owner_coro_id) == NO_OWNER_ID);
}

MACHINE_API int machine_mutex_lock(machine_mutex_t *mutex, uint32_t timeout_ms)
{
	uint64_t start_ms = machine_time_ms();
	uint64_t current_coro_id = get_current_coro_id();

	unsigned int spin_count = 0U;
	int delay_idx = 0;
	const int delays_count = sizeof(sleep_durations_ms) / sizeof(uint32_t);

	for (;;) {
		if (mm_atomic_u64_cas(&mutex->owner_coro_id, NO_OWNER_ID,
				      current_coro_id)) {
			break;
		}

		MM_MUTEX_BACKOFF;

		if (++spin_count > MAX_SPIN_COUNT) {
			machine_sleep(sleep_durations_ms[delay_idx]);
			delay_idx = (delay_idx + 1) % delays_count;

			/* check there, because spins works fast */
			if ((machine_time_ms() - start_ms) > timeout_ms) {
				return 0;
			}
		}
	}

	return 1;
}

MACHINE_API void machine_mutex_unlock(machine_mutex_t *mutex)
{
	uint64_t current_coro_id = get_current_coro_id();

	if (!mm_atomic_u64_cas(&mutex->owner_coro_id, current_coro_id,
			       NO_OWNER_ID)) {
		/*
         * we do not held this mutex
         * seems like this is critical and hard to find bug
         */
		abort();
	}
}

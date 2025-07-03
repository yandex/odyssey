#pragma once

#include <stdatomic.h>

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

typedef atomic_uint mm_sleeplock_t;

#if defined(__x86_64__) || defined(__i386) || defined(_X86_)
#define MM_SLEEPLOCK_BACKOFF __asm__("pause")
#else
#define MM_SLEEPLOCK_BACKOFF
#endif

static inline void mm_sleeplock_init(mm_sleeplock_t *lock)
{
	atomic_init(lock, 0);
}

static inline void mm_sleeplock_lock(mm_sleeplock_t *lock)
{
	unsigned int spin_count = 0U;
	while (atomic_exchange(lock, 1) != 0) {
		MM_SLEEPLOCK_BACKOFF;
		if (++spin_count > 30U) {
			usleep(1);
		}
	}
}

static inline void mm_sleeplock_unlock(mm_sleeplock_t *lock)
{
	atomic_exchange(lock, 0);
}

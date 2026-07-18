#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#if defined(__APPLE__)

#include <os/lock.h>

typedef os_unfair_lock mm_spinlock_t;

static inline void mm_spinlock_init(mm_spinlock_t *lock)
{
	*lock = OS_UNFAIR_LOCK_INIT;
}

static inline void mm_spinlock_lock(mm_spinlock_t *lock)
{
	os_unfair_lock_lock(lock);
}

static inline void mm_spinlock_unlock(mm_spinlock_t *lock)
{
	os_unfair_lock_unlock(lock);
}

static inline void mm_spinlock_destroy(mm_spinlock_t *lock)
{
	(void)lock;
}

#else /* Linux and other POSIX with pthread spinlock support */

#include <pthread.h>

typedef pthread_spinlock_t mm_spinlock_t;

static inline void mm_spinlock_init(mm_spinlock_t *lock)
{
	pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE);
}

static inline void mm_spinlock_lock(mm_spinlock_t *lock)
{
	pthread_spin_lock(lock);
}

static inline void mm_spinlock_unlock(mm_spinlock_t *lock)
{
	pthread_spin_unlock(lock);
}

static inline void mm_spinlock_destroy(mm_spinlock_t *lock)
{
	pthread_spin_destroy(lock);
}

#endif

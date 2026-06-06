#pragma once

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

#include <machinarium/mutex.h>

typedef struct kiwi_params_lock kiwi_params_lock_t;

struct kiwi_params_lock {
	mm_mutex_t lock;
	kiwi_params_t params;
};

static inline void kiwi_params_lock_init(kiwi_params_lock_t *pl)
{
	mm_mutex_init(&pl->lock);
	kiwi_params_init(&pl->params);
}

static inline void kiwi_params_lock_free(kiwi_params_lock_t *pl)
{
	mm_mutex_destroy(&pl->lock);
	kiwi_params_free(&pl->params);
}

static inline int kiwi_params_lock_count(kiwi_params_lock_t *pl)
{
	mm_mutex_lock2(&pl->lock);
	int rc = pl->params.count;
	mm_mutex_unlock(&pl->lock);
	return rc;
}

static inline int kiwi_params_lock_copy(kiwi_params_lock_t *pl,
					kiwi_params_t *dest)
{
	mm_mutex_lock2(&pl->lock);
	int rc;
	rc = kiwi_params_copy(dest, &pl->params);
	mm_mutex_unlock(&pl->lock);
	return rc;
}

static inline int kiwi_params_lock_set_once(kiwi_params_lock_t *pl,
					    kiwi_params_t *params)
{
	mm_mutex_lock2(&pl->lock);
	if (pl->params.count > 0) {
		mm_mutex_unlock(&pl->lock);
		return 0;
	}
	pl->params = *params;
	mm_mutex_unlock(&pl->lock);
	return 1;
}

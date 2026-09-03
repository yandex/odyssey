#include <stdatomic.h>
#include <string.h>

#include <machinarium/ds/intrusive/lf-stack.h>

#if MM_LF_STACK_TAGGED

/* ---- lock-free path: 128-bit tagged pointer ---- */

void mm_lf_stack_init(mm_lf_stack_t *st)
{
	mm_lf_stack_head_t empty = { 0 };
	atomic_init(&st->head, empty);
}

void mm_lf_stack_destroy(mm_lf_stack_t *st)
{
	(void)st;
}

int mm_lf_stack_push(mm_lf_stack_t *st, mm_lf_stack_entry_t *e)
{
	mm_lf_stack_head_t old;
	mm_lf_stack_head_t neu;

	old = atomic_load_explicit(&st->head, memory_order_relaxed);

	for (;;) {
		e->prev = (mm_lf_stack_entry_t *)old.ptr;
		neu.ptr = (uintptr_t)e;
		neu.tag = old.tag + 1;

		if (atomic_compare_exchange_weak_explicit(
			    &st->head, &old, neu, memory_order_release,
			    memory_order_relaxed)) {
			break;
		}
	}

	return 0;
}

mm_lf_stack_entry_t *mm_lf_stack_pop(mm_lf_stack_t *st)
{
	mm_lf_stack_head_t old;
	mm_lf_stack_head_t neu;

	old = atomic_load_explicit(&st->head, memory_order_acquire);

	for (;;) {
		if (old.ptr == 0) {
			return NULL;
		}

		mm_lf_stack_entry_t *entry = (mm_lf_stack_entry_t *)old.ptr;
		neu.ptr = (uintptr_t)entry->prev;
		neu.tag = old.tag + 1;

		if (atomic_compare_exchange_weak_explicit(
			    &st->head, &old, neu, memory_order_acquire,
			    memory_order_relaxed)) {
			return entry;
		}
	}
}

#else

/* ---- fallback path: spinlock ---- */

void mm_lf_stack_init(mm_lf_stack_t *st)
{
	memset(st, 0, sizeof(mm_lf_stack_t));
	mm_spinlock_init(&st->lock);
}

void mm_lf_stack_destroy(mm_lf_stack_t *st)
{
	(void)st;
}

int mm_lf_stack_push(mm_lf_stack_t *st, mm_lf_stack_entry_t *e)
{
	mm_spinlock_lock(&st->lock);
	e->prev = st->head;
	st->head = e;
	mm_spinlock_unlock(&st->lock);
	return 0;
}

mm_lf_stack_entry_t *mm_lf_stack_pop(mm_lf_stack_t *st)
{
	mm_lf_stack_entry_t *e;

	mm_spinlock_lock(&st->lock);
	e = st->head;
	if (e) {
		st->head = e->prev;
	}
	mm_spinlock_unlock(&st->lock);

	return e;
}

#endif

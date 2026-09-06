#include <string.h>

#include <machinarium/ds/intrusive/lf-stack.h>

#if MM_LF_STACK_TAGGED

/* ---- lock-free path: 128-bit tagged pointer via __sync builtins ---- */

static inline mm_int128 mm_lf_head_pack(mm_lf_stack_head_t h)
{
	union {
		mm_lf_stack_head_t s;
		mm_int128 raw;
	} u;
	u.s = h;
	return u.raw;
}

static inline mm_lf_stack_head_t mm_lf_head_unpack(mm_int128 raw)
{
	union {
		mm_int128 raw;
		mm_lf_stack_head_t s;
	} u;
	u.raw = raw;
	return u.s;
}

void mm_lf_stack_init(mm_lf_stack_t *st)
{
	st->raw = 0;
}

void mm_lf_stack_destroy(mm_lf_stack_t *st)
{
	(void)st;
}

int mm_lf_stack_push(mm_lf_stack_t *st, mm_lf_stack_entry_t *e)
{
	mm_int128 old_raw = st->raw;

	for (;;) {
		mm_lf_stack_head_t old = mm_lf_head_unpack(old_raw);
		e->prev = (mm_lf_stack_entry_t *)old.ptr;

		mm_lf_stack_head_t neu;
		neu.ptr = (uintptr_t)e;
		neu.tag = old.tag + 1;

		mm_int128 neu_raw = mm_lf_head_pack(neu);

		mm_int128 prev_raw =
			__sync_val_compare_and_swap(&st->raw, old_raw, neu_raw);

		if (prev_raw == old_raw) {
			break;
		}

		/* CAS failed: prev_raw holds the current head. Retry. */
		old_raw = prev_raw;
	}

	return 0;
}

mm_lf_stack_entry_t *mm_lf_stack_pop(mm_lf_stack_t *st)
{
	mm_int128 old_raw = st->raw;

	for (;;) {
		mm_lf_stack_head_t old = mm_lf_head_unpack(old_raw);
		if (old.ptr == 0) {
			return NULL;
		}

		mm_lf_stack_entry_t *entry = (mm_lf_stack_entry_t *)old.ptr;

		mm_lf_stack_head_t neu;
		neu.ptr = (uintptr_t)entry->prev;
		neu.tag = old.tag + 1;

		mm_int128 neu_raw = mm_lf_head_pack(neu);

		mm_int128 prev_raw =
			__sync_val_compare_and_swap(&st->raw, old_raw, neu_raw);

		if (prev_raw == old_raw) {
			return entry;
		}

		/* CAS failed: retry with the current head. */
		old_raw = prev_raw;
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

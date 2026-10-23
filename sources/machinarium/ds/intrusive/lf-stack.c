#include <string.h>

#include <machinarium/ds/intrusive/lf-stack.h>

#if MM_LF_STACK_TAGGED

/* ---- lock-free path: 128-bit tagged pointer via stdatomic ---- */

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
	atomic_init(&st->raw, (mm_int128)0);
}

void mm_lf_stack_destroy(mm_lf_stack_t *st)
{
	(void)st;
}

int mm_lf_stack_push(mm_lf_stack_t *st, mm_lf_stack_entry_t *e)
{
	/* bootstrapped by the CAS below: the initial value is a pure hint,
	 * so the stack performs no 16-byte atomic load (gcc does not inline
	 * it and would pull in libatomic). on CAS failure the current head
	 * is stored back into old_raw
	 */
	mm_int128 old_raw = 0;

	for (;;) {
		mm_lf_stack_head_t old = mm_lf_head_unpack(old_raw);
		atomic_store_explicit(&e->prev,
				      (struct mm_lf_stack_entry *)old.ptr,
				      memory_order_relaxed);

		mm_lf_stack_head_t neu;
		neu.ptr = (uintptr_t)e;
		neu.tag = old.tag + 1;

		mm_int128 neu_raw = mm_lf_head_pack(neu);

		/* release: publish e->prev together with the new head */
		if (atomic_compare_exchange_strong_explicit(
			    &st->raw, &old_raw, neu_raw, memory_order_release,
			    memory_order_relaxed)) {
			break;
		}

		/* CAS failed: old_raw holds the current head. Retry. */
	}

	return 0;
}

mm_lf_stack_entry_t *mm_lf_stack_pop(mm_lf_stack_t *st)
{
	/* bootstrapped by the CAS below: no 16-byte atomic load, the
	 * current head comes from the CAS itself. acquire on CAS failure
	 * pairs with the release CAS of the push, so that entry->prev
	 * written before that push is visible below
	 */
	mm_int128 old_raw = 0;

	for (;;) {
		mm_lf_stack_head_t old = mm_lf_head_unpack(old_raw);
		mm_lf_stack_entry_t *entry = (mm_lf_stack_entry_t *)old.ptr;

		mm_lf_stack_head_t neu;
		/* the bootstrap iteration has no entry yet: the CAS below
		 * confirms the stack is empty and returns NULL
		 */
		neu.ptr = entry ? (uintptr_t)atomic_load_explicit(
					  &entry->prev, memory_order_relaxed) :
				  0;
		neu.tag = old.tag + 1;

		mm_int128 neu_raw = mm_lf_head_pack(neu);

		/* release: publish the new head; acquire on failure so the
		 * retried iteration sees entry->prev of the new head
		 */
		if (atomic_compare_exchange_strong_explicit(
			    &st->raw, &old_raw, neu_raw, memory_order_release,
			    memory_order_acquire)) {
			return entry;
		}

		/* CAS failed: retry with the current head. */
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

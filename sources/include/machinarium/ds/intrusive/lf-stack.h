#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * lock-free intrusive stack (Treiber stack) with ABA protection.
 *
 * When the platform supports lock-free 128-bit (16-byte) CAS, a tagged
 * pointer is used: each head update increments a version counter, so a
 * stale pointer that matches the old head will fail the CAS because the
 * tag differs. This is lock-free on x86-64 (cmpxchg16b, -mcx16) and
 * aarch64 (casp/ldxp-stxp).
 *
 * When 128-bit CAS is not lock-free (e.g. 32-bit platforms, or x86-64
 * built without -mcx16), the stack falls back to a spinlock. This is
 * still O(1) push/pop at least
 */

#include <stdatomic.h>
#include <stdint.h>

typedef struct mm_lf_stack_entry {
	struct mm_lf_stack_entry *prev;
} mm_lf_stack_entry_t;

#if defined(__SIZEOF_INT128__) && defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)

/* lock-free path: 128-bit tagged pointer */

#define MM_LF_STACK_TAGGED 1

typedef struct {
	uintptr_t ptr;
	uint64_t tag;
} mm_lf_stack_head_t;

typedef struct {
	_Atomic mm_lf_stack_head_t head;
} mm_lf_stack_t;

#else

/* fallback path: spinlock */

#define MM_LF_STACK_TAGGED 0

#include <machinarium/spinlock.h>

typedef struct {
	mm_lf_stack_entry_t *head;
	mm_spinlock_t lock;
} mm_lf_stack_t;

#endif

void mm_lf_stack_init(mm_lf_stack_t *st);
void mm_lf_stack_destroy(mm_lf_stack_t *st);

int mm_lf_stack_push(mm_lf_stack_t *st, mm_lf_stack_entry_t *e);
mm_lf_stack_entry_t *mm_lf_stack_pop(mm_lf_stack_t *st);

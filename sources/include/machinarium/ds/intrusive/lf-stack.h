#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * lock-free intrusive stack (Treiber stack) with ABA protection.
 *
 * nb: on x86-64 with -mcx16 the compiled code requires a CPU that
 * supports cmpxchg16b (all processors since ~2008). Running on an
 * older CPU will SIGILL. This is a deployment assumption, not a runtime
 * check.
 */

#include <stdint.h>

typedef struct mm_lf_stack_entry {
	struct mm_lf_stack_entry *prev;
} mm_lf_stack_entry_t;

#if defined(__SIZEOF_INT128__) && defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)

/* lock-free path: 128-bit tagged pointer via __sync builtins */

#define MM_LF_STACK_TAGGED 1

typedef __int128_t mm_int128;

typedef struct {
	uintptr_t ptr;
	uint64_t tag;
} mm_lf_stack_head_t;

typedef struct {
	mm_int128 raw;
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

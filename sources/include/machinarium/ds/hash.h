#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdint.h>
#include <stddef.h>

typedef uint64_t mm_hash_t;

/* seed is optional, can be zero */
mm_hash_t mm_xxh64_hash(const void *data, size_t size, uint64_t seed);

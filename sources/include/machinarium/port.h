
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * portability helpers: compiler and platform specific bits that do not
 * deserve a separate header
 */

#pragma once

#include <assert.h>
#include <stdint.h>

/*
 * Index of the least significant set bit, undefined for zero input.
 */
static inline int mm_ctz64(uint64_t bits)
{
	assert(bits != 0);

#if defined(__GNUC__) || defined(__clang__)
	return __builtin_ctzll(bits);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
	unsigned long index;

	_BitScanForward64(&index, bits);
	return (int)index;
#else
	int index = 0;

	while ((bits & 1) == 0) {
		bits >>= 1;
		index++;
	}
	return index;
#endif
}

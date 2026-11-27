
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

/* Native poll backend. */
#if defined(__linux__)
#define MM_HAVE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__) || defined(__DragonFly__)
#define MM_HAVE_KQUEUE 1
#else
#error "machinarium: unsupported platform, no poll backend available"
#endif

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

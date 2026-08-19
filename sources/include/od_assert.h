#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * od_assert — assert that logs through logger before aborting
 *
 * unlike the standard assert(), the failure message is written to the
 * configured log destination (file / syslog) so it is not lost when
 * stderr is unavailable
 */

#include <stdlib.h>

extern void od_assert_fail(const char *expr, const char *file, int line);

#ifdef NDEBUG
#define od_assert(expr) ((void)1)
#else
#define od_assert(expr)                                            \
	do {                                                       \
		if (od_unlikely(!(expr))) {                        \
			od_assert_fail(#expr, __FILE__, __LINE__); \
		}                                                  \
	} while (0)
#endif

#define od_release_assert(expr)                                    \
	do {                                                       \
		if (od_unlikely(!(expr))) {                        \
			od_assert_fail(#expr, __FILE__, __LINE__); \
		}                                                  \
	} while (0)

#ifndef ODYSSEY_DEFER_H
#define ODYSSEY_DEFER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#if defined(__GNUC__) || defined(__clang__)

typedef void (*defer_fn)(void *);

typedef struct {
	defer_fn function;
	void *argument;
} defer_info_t;

static inline void _od_defer_caller(defer_info_t *info)
{
	if (info) {
		info->function(info->argument);
	}
}

#define __OD_DEFER_CONCAT(a, b) a##b
#define _OD_DEFER_CONCAT(a, b) __OD_DEFER_CONCAT(a, b)
#define od_defer(fn, arg)                                                   \
	__attribute__((cleanup(_od_defer_caller)))                          \
		defer_info_t _OD_DEFER_CONCAT(_defer_on_line, __LINE__) = { \
			.function = (fn), .argument = (arg)                 \
		}

#else

#error defer is not implemented for this compiler

#endif /* __GNUC__ or __clang__ */

#endif /* ODYSSEY_DEFER_H */
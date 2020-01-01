#ifndef ODYSSEY_UTIL_H
#define ODYSSEY_UTIL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

static inline int
od_vsnprintf(char *buf, int size, char *fmt, va_list args)
{
	int rc;
	rc = vsnprintf(buf, size, fmt, args);
	if (od_unlikely(rc >= size))
		rc = (size - 1);
	return rc;
}

static inline int
od_snprintf(char *buf, int size, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc;
	rc = od_vsnprintf(buf, size, fmt, args);
	va_end(args);
	return rc;
}

static inline int
od_dyn_sprintf(char **buffer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc;
	rc = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	*buffer = malloc(rc + 1);
	if (!*buffer) {
		return rc;
	}
	va_start(args, fmt);
	vsprintf(*buffer, fmt, args);
	va_end(args);

	return rc;
}

static inline int
od_strmemcmp(char *null_terminated, char *other, size_t other_size)
{
	char *other_end = other + other_size;
	while (*null_terminated
		   && other < other_end
		   && *null_terminated == *other) {
		++null_terminated;
		++other;
    }

    if (other != other_end)
        return *other - *null_terminated;

    return -*null_terminated;
}

#endif /* ODYSSEY_UTIL_H */

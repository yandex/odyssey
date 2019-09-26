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
od_strmemcmp(char *null_terminated, char *other, size_t other_size)
{
    int memcmp_result = memcmp(null_terminated, other, other_size);
    if (memcmp_result != 0)
    {
        return memcmp_result;
    }

    return -null_terminated[other_size];
}

#endif /* ODYSSEY_UTIL_H */

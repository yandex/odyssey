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
od_snprintf(char *buf, size_t size, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc;
	rc = od_vsnprintf(buf, size, fmt, args);
	va_end(args);
	return rc;
}

#endif /* ODYSSEY_UTIL_H */

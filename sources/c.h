
#ifndef ODYSSEY_C_H
#define ODYSSEY_C_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <string.h>

#include <errno.h>
#include <signal.h>

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <fcntl.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <pid.h>
#include <stdarg.h>
#include <math.h>
#include <stddef.h>

#ifdef USE_SSL
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

/* only GCC supports the unused attribute */
#ifdef __GNUC__
#define od_attribute_unused() __attribute__((unused))
#else
#define od_attribute_unused()
#endif

/* GCC support aligned, packed and noreturn */
#ifdef __GNUC__
#define od_attribute_aligned(a) __attribute__((aligned(a)))
#define od_attribute_noreturn() __attribute__((noreturn))
#define od_attribute_packed() __attribute__((packed))
#endif

#define FLEXIBLE_ARRAY_MEMBER /* empty */

#if defined __has_builtin
#if __has_builtin(__builtin_unreachable) /* odyssey unreachable code */
#define od_unreachable() __builtin_unreachable()
#endif
#endif

#ifndef od_unreachable
#define od_unreachable() abort()
#endif

#endif // ODYSSEY_C_H

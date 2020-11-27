#ifndef ODYSSEY_MACRO_H
#define ODYSSEY_MACRO_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#define USE_SCRAM

#define od_likely(EXPR)   __builtin_expect(!!(EXPR), 1)
#define od_unlikely(EXPR) __builtin_expect(!!(EXPR), 0)

#define od_container_of(N, T, F) ((T *)((char *)(N) - __builtin_offsetof(T, F)))

#define OK_RESPONSE     0
#define NOT_OK_RESPONSE -1

#define _STRINGIFY(x) #x
#define STRINGIFY(x)  _STRINGIFY(x)
#define CONCAT_(A, B) A##B
#define CONCAT(A, B)  CONCAT_(A, B)

typedef int od_retcode_t;

#endif /* ODYSSEY_MACRO_H */

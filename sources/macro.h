#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#define od_likely(EXPR) __builtin_expect(!!(EXPR), 1)
#define od_unlikely(EXPR) __builtin_expect(!!(EXPR), 0)

#define OK_RESPONSE 0
#define NOT_OK_RESPONSE -1

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)

static const uint64_t interval_usec = 1000000ull;

typedef int od_retcode_t;

/*misc*/

#define INVALID_COROUTINE_ID -1

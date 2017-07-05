#ifndef OD_MACRO_H
#define OD_MACRO_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#define od_likely(EXPR)   __builtin_expect(!! (EXPR), 1)
#define od_unlikely(EXPR) __builtin_expect(!! (EXPR), 0)

#define od_container_of(N, T, F) \
	((T*)((char*)(N) - __builtin_offsetof(T, F)))

#endif /* OD_MACRO_H */

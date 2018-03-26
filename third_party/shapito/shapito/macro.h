#ifndef SHAPITO_MACRO_H
#define SHAPITO_MACRO_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#define SHAPITO_API

#define shapito_likely(expr)   __builtin_expect(!! (expr), 1)
#define shapito_unlikely(expr) __builtin_expect(!! (expr), 0)
#define shapito_packed         __attribute__((packed))

#define shapito_container_of(ptr, type, field) \
	((type*)((char*)(ptr) - __builtin_offsetof(type, field)))

#endif /* SHAPITO_MACRO_H */

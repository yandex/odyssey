#ifndef SO_MACRO_H_
#define SO_MACRO_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

#define so_likely(e)   __builtin_expect(!! (e), 1)
#define so_unlikely(e) __builtin_expect(!! (e), 0)
#define so_packed      __attribute__((packed))
#define so_container_of(ptr, t, f) \
	((t*)((char*)(ptr) - __builtin_offsetof(t, f)))

#endif

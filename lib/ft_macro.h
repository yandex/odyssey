#ifndef FT_MACRO_H_
#define FT_MACRO_H_

/*
 * libfluent.
 *
 * Cooperative multitasking engine.
*/

#define ft_container_of(ptr, t, f) \
	((t*)((char*)(ptr) - __builtin_offsetof(t, f)))

#endif

#ifndef MM_MACRO_H
#define MM_MACRO_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#define mm_container_of(ptr, t, f) \
	((t*)((char*)(ptr) - __builtin_offsetof(t, f)))

#endif /* MM_MACRO_H */

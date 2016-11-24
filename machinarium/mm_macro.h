#ifndef MM_MACRO_H_
#define MM_MACRO_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#define mm_container_of(ptr, t, f) \
	((t*)((char*)(ptr) - __builtin_offsetof(t, f)))

#endif

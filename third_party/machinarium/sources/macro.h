#ifndef MM_MACRO_H
#define MM_MACRO_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#define mm_container_of(ptr, type, field) \
	((type*)((char*)(ptr) - __builtin_offsetof(type, field)))

#define mm_cast(type, ptr) \
	((type)(ptr))

#endif /* MM_MACRO_H */

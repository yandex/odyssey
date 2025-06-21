#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#define machine_container_of(ptr, type, field) \
	((type *)((char *)(ptr) - __builtin_offsetof(type, field)))

typedef int mm_retcode_t;

#define MM_OK_RETCODE 0
#define MM_NOTOK_RETCODE -1

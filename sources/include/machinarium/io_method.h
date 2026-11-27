#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <string.h>

#include <machinarium/build.h>
#include <machinarium/port.h>

#if defined(MM_HAVE_EPOLL)
#define MM_DEFAULT_IO_METHOD "epoll"
#elif defined(MM_HAVE_KQUEUE)
#define MM_DEFAULT_IO_METHOD "kqueue"
#endif

#ifdef HAVE_IO_URING
#define MM_AVAILABLE_IO_METHODS MM_DEFAULT_IO_METHOD ", io_uring"
#else
#define MM_AVAILABLE_IO_METHODS MM_DEFAULT_IO_METHOD
#endif

/* Return a name with static storage duration for each supported method. */
static inline const char *mm_io_method_parse(const char *name)
{
	if (name == NULL) {
		return NULL;
	}
	if (strcmp(name, MM_DEFAULT_IO_METHOD) == 0) {
		return MM_DEFAULT_IO_METHOD;
	}
#ifdef HAVE_IO_URING
	if (strcmp(name, "io_uring") == 0) {
		return "io_uring";
	}
#endif
	return NULL;
}

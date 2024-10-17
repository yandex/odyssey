#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <machinarium.h>

#include <stdio.h>
#include <dlfcn.h>
#include <execinfo.h>

MACHINE_API int machine_get_backtrace(void **entries, int max)
{
	return backtrace(entries, max);
}

#define MM_BACKTRACE_STRING_N_ENTRIES 15

__thread char backtrace_string[MM_BACKTRACE_STRING_N_ENTRIES * 40];

MACHINE_API const char *machine_get_backtrace_string()
{
	void *bt[MM_BACKTRACE_STRING_N_ENTRIES];
	int nentries = machine_get_backtrace(bt, MM_BACKTRACE_STRING_N_ENTRIES);

	if (nentries <= 0) {
		return NULL;
	}

	char *wptr = backtrace_string;
	for (int i = 0; i < nentries; ++i) {
		wptr += sprintf(wptr, "%p ", bt[i]);
	}

	wptr += sprintf(wptr, "(");

	for (int i = 0; i < nentries; ++i) {
		void *addr = bt[i];

		Dl_info info;
		if (dladdr(addr, &info) == 0) {
			wptr += sprintf(wptr, "[unknown]");
		} else {
			void *calibrated = (void *)((uintptr_t)addr -
						    (uintptr_t)info.dli_fbase);

			wptr += sprintf(wptr, "%p", calibrated);
		}

		if (i != nentries - 1) {
			wptr += sprintf(wptr, " ");
		}
	}

	wptr += sprintf(wptr, ")");
	*wptr = '\0';

	return backtrace_string;
}

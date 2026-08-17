#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <setproctitle.h>
#include <util.h>

od_retcode_t od_setproctitlef(char **argv_ptr, int argv_len, char *fmt, ...)
{
	if (argv_ptr == NULL || *argv_ptr == NULL || argv_len <= 0) {
		return NOT_OK_RESPONSE;
	}

	char title[OD_MAX_PROC_TITLE_LEN];
	va_list args;
	va_start(args, fmt);
	int title_len = od_vsnprintf(title, sizeof(title), fmt, args);
	va_end(args);

	if (title_len < 0) {
		return NOT_OK_RESPONSE;
	}

	title[title_len] = '\0';

	memset(*argv_ptr, 0, argv_len);

	size_t copy_len = title_len;
	if (copy_len > (size_t)argv_len - 1) {
		copy_len = (size_t)argv_len - 1;
	}
	memcpy(*argv_ptr, title, copy_len + 1);

	return OK_RESPONSE;
}

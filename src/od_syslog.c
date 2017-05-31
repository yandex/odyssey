
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <syslog.h>

#include <time.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "od_macro.h"
#include "od_syslog.h"

typedef struct od_syslog_facility od_syslog_facility_t;

struct od_syslog_facility
{
	char *name;
	int   id;
};

od_syslog_facility_t od_syslog_facilities[] =
{
	{ "daemon", LOG_DAEMON },
	{ "user",   LOG_USER   },
	{ "local0", LOG_LOCAL0 },
	{ "local1", LOG_LOCAL1 },
	{ "local2", LOG_LOCAL2 },
	{ "local3", LOG_LOCAL3 },
	{ "local4", LOG_LOCAL4 },
	{ "local5", LOG_LOCAL5 },
	{ "local6", LOG_LOCAL6 },
	{ "local7", LOG_LOCAL7 },
	{  NULL,    0 }
};

static int od_syslog_prio[] = {
	LOG_INFO, LOG_ERR, LOG_DEBUG
};

void od_syslog_init(od_syslog_t *syslog)
{
	syslog->in_use = 0;
}

int od_syslog_open(od_syslog_t *slog, char *ident, char *facility)
{
	int facility_id = LOG_DAEMON;
	if (facility) {
		int i = 0;
		od_syslog_facility_t *facility_ptr;
		for (;;) {
			facility_ptr = &od_syslog_facilities[i];
			if (facility_ptr->name == NULL)
				break;
			if (strcasecmp(facility_ptr->name, facility) == 0) {
				facility_id = facility_ptr->id;
				break;
			}
			i++;
		}
	}
	slog->in_use = 1;
	if (ident == NULL)
		ident = "odissey";
	openlog(ident, 0, facility_id);
	return 0;
}

void od_syslog_close(od_syslog_t *slog)
{
	if (! slog->in_use)
		return;
	closelog();
}

void od_syslog(od_syslog_t *slog, od_syslogprio_t prio, char *msg)
{
	if (slog->in_use)
		syslog(od_syslog_prio[prio], "%s", msg);
}


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

#include <syslog.h>

#include <time.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "od_macro.h"
#include "od_syslog.h"

typedef struct odsyslog_facility_t odsyslog_facility_t;

struct odsyslog_facility_t {
	char *name;
	int   id;
};

odsyslog_facility_t od_syslog_facilities[] =
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

int od_syslog_open(odsyslog_t *slog, char *ident, char *facility)
{
	int facility_id = LOG_DAEMON;
	if (facility) {
		int i = 0;
		odsyslog_facility_t *facility_ptr;
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

void od_syslog_close(odsyslog_t *slog)
{
	if (! slog->in_use)
		return;
	closelog();
}

void od_syslog(odsyslog_t *slog, odsyslog_prio_t prio, char *msg)
{
	if (slog->in_use)
		syslog(od_syslog_prio[prio], "%s", msg);
}

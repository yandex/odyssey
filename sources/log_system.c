
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <syslog.h>

#include "sources/macro.h"
#include "sources/log_system.h"

typedef struct od_logsystem_facility od_logsystem_facility_t;

struct od_logsystem_facility
{
	char *name;
	int   id;
};

od_logsystem_facility_t od_logsystem_facilities[] =
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

static int od_logsystem_prio[] = {
	LOG_INFO, LOG_ERR, LOG_DEBUG
};

void od_logsystem_init(od_logsystem_t *log)
{
	log->in_use = 0;
}

int od_logsystem_open(od_logsystem_t *log, char *ident, char *facility)
{
	int facility_id = LOG_DAEMON;
	if (facility) {
		int i = 0;
		od_logsystem_facility_t *facility_ptr;
		for (;;) {
			facility_ptr = &od_logsystem_facilities[i];
			if (facility_ptr->name == NULL)
				break;
			if (strcasecmp(facility_ptr->name, facility) == 0) {
				facility_id = facility_ptr->id;
				break;
			}
			i++;
		}
	}
	log->in_use = 1;
	if (ident == NULL)
		ident = "odissey";
	openlog(ident, 0, facility_id);
	return 0;
}

void od_logsystem_close(od_logsystem_t *log)
{
	if (! log->in_use)
		return;
	closelog();
}

void od_logsystem(od_logsystem_t *log, od_logsystem_prio_t prio, char *msg, int len)
{
	if (! log->in_use)
		return;
	syslog(od_logsystem_prio[prio], "%.*s", len, msg);
}

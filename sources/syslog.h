#ifndef OD_SYSLOG_H
#define OD_SYSLOG_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_syslog od_syslog_t;

typedef enum
{
	OD_SYSLOG_INFO,
	OD_SYSLOG_ERROR,
	OD_SYSLOG_DEBUG
} od_syslogprio_t;

struct od_syslog
{
	int in_use;
};

void od_syslog_init(od_syslog_t*);
int  od_syslog_open(od_syslog_t*, char*, char*);
void od_syslog_close(od_syslog_t*);
void od_syslog(od_syslog_t*, od_syslogprio_t, char*);

#endif /* OD_SYSLOG_H */

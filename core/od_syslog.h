#ifndef OD_SYSLOG_H_
#define OD_SYSLOG_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odsyslog_t odsyslog_t;

typedef enum {
	OD_SYSLOG_INFO,
	OD_SYSLOG_ERROR,
	OD_SYSLOG_DEBUG
} odsyslog_prio_t;

struct odsyslog_t {
	int in_use;
};

static inline void
od_syslog_init(odsyslog_t *syslog) {
	syslog->in_use = 0;
}

int  od_syslog_open(odsyslog_t*, char*, char*);
void od_syslog_close(odsyslog_t*);
void od_syslog(odsyslog_t*, odsyslog_prio_t, char*);

#endif

#ifndef OD_LOG_SYSTEM_H
#define OD_LOG_SYSTEM_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_logsystem od_logsystem_t;

typedef enum
{
	OD_LOGSYSTEM_INFO,
	OD_LOGSYSTEM_ERROR,
	OD_LOGSYSTEM_DEBUG
} od_logsystem_prio_t;

struct od_logsystem
{
	int in_use;
};

void od_logsystem_init(od_logsystem_t*);
int  od_logsystem_open(od_logsystem_t*, char*, char*);
void od_logsystem_close(od_logsystem_t*);
void od_logsystem(od_logsystem_t*, od_logsystem_prio_t, char*, int);

#endif

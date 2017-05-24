#ifndef OD_PID_H
#define OD_PID_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_pid od_pid_t;

struct od_pid
{
	pid_t pid;
};

void od_pidinit(od_pid_t*);
int  od_pidfile_create(od_pid_t*, char*);
int  od_pidfile_unlink(od_pid_t*, char*);

#endif

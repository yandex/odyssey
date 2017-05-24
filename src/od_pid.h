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

void od_pid_init(od_pid_t*);
int  od_pid_create(od_pid_t*, char*);
int  od_pid_unlink(od_pid_t*, char*);

#endif

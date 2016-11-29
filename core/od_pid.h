#ifndef OD_PID_H_
#define OD_PID_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_pid_t od_pid_t;

struct od_pid_t {
	pid_t pid;
};

void od_pidinit(od_pid_t*);
int  od_pidfile_create(od_pid_t*, char*);
int  od_pidfile_unlink(od_pid_t*, char*);

#endif

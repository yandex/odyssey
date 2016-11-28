#ifndef OD_PID_H_
#define OD_PID_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odpid_t odpid_t;

struct odpid_t {
	pid_t pid;
};

void od_pidinit(odpid_t*);
int  od_pidfile_create(odpid_t*, char*);
int  od_pidfile_unlink(odpid_t*, char*);

#endif

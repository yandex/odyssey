#ifndef OD_INSTANCE_H_
#define OD_INSTANCE_H_

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_instance od_instance_t;

struct od_instance
{
	od_pid_t        pid;
	od_syslog_t     syslog;
	od_log_t        log;
	od_idmgr_t      id_mgr;
	od_schememgr_t  scheme_mgr;
	od_scheme_t     scheme;
	char           *config_file;
};

void od_instance_init(od_instance_t*);
void od_instance_free(od_instance_t*);
int  od_instance_main(od_instance_t*, int, char**);

#endif /* OD_INSTANCE_H */

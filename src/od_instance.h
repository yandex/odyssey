#ifndef OD_INTANCE_H_
#define OD_INTANCE_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_intance od_instance_t;

struct od_intance
{
	od_pid_t    pid;
	od_syslog_t syslog;
	od_log_t    log;
	od_config_t config;
	od_scheme_t scheme;
};

void od_instance_init(od_instance_t*);
void od_instance_free(od_instance_t*);
int  od_instance_main(od_instance_t*, int, char**);

#endif /* OD_H */

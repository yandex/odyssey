#ifndef OD_INSTANCE_H_
#define OD_INSTANCE_H_

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_instance od_instance_t;

struct od_instance
{
	od_pid_t        pid;
	od_logger_t     logger;
	od_idmgr_t      id_mgr;
	od_config_t     config;
	shapito_cache_t stream_cache;
	int             is_shared;
	char           *config_file;
};

void od_instance_init(od_instance_t*);
void od_instance_free(od_instance_t*);
int  od_instance_main(od_instance_t*, int, char**);

#endif /* OD_INSTANCE_H */

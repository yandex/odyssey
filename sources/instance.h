#ifndef ODYSSEY_INSTANCE_H
#define ODYSSEY_INSTANCE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_instance od_instance_t;

struct od_instance
{
	od_pid_t     pid;
	od_logger_t  logger;
	char        *config_file;
	od_config_t	*config;

	mcxt_context_t	top_mcxt;
};

void od_instance_init(od_instance_t*);
void od_instance_free(od_instance_t*);
int  od_instance_main(od_instance_t*, int, char**);

#endif /* ODYSSEY_INSTANCE_H */

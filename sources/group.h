/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#ifndef ODYSSEY_GROUP_CHECK_ITER_INTERVAL
#define ODYSSEY_GROUP_CHECK_ITER_INTERVAL 500 // ms

typedef struct od_group od_group_t;

struct od_group {
	char *route_usr;
	char *route_db;

	char *storage_user;
	char *storage_db;
	char *group_name;

	char *group_query;
	int check_retry;
	int online;

	od_global_t *global;

	od_list_t link;
};

int od_group_free(od_group_t *);
void od_group_qry_format(char *, char *, ...);
int od_group_parse_val_datarow(machine_msg_t *, int *);

#endif /* ODYSSEY_GROUP_CHECK_ITER_INTERVAL */
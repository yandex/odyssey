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
	char *group_query_user;
	char *group_query_db;
	int check_retry;
	int online;

	od_global_t *global;

	od_list_t link;
};

typedef struct od_group_member_name_item od_group_member_name_item_t;

struct od_group_member_name_item {
	char *value;
	int is_checked;
	od_list_t link;
};

int od_group_free(od_group_t *);
int od_group_parse_val_datarow(machine_msg_t *, char **);
od_group_member_name_item_t *od_group_member_name_item_add(od_list_t *);

#endif /* ODYSSEY_GROUP_CHECK_ITER_INTERVAL */
#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

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

	machine_list_t link;
};

typedef struct od_group_member_name_item od_group_member_name_item_t;

struct od_group_member_name_item {
	char *value;
	int is_checked;
	machine_list_t link;
};

int od_group_free(od_group_t *);
int od_group_parse_val_datarow(machine_msg_t *, char **);
od_group_member_name_item_t *od_group_member_name_item_add(machine_list_t *);

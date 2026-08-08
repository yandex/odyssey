#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Minimal SQL parser AST definitions.
 */

#include <alloc/linear.h>
#include <id.h>

typedef enum {
	OD_SQL_MINIMAL_NODE_TYPE_INVALID = 0,
	OD_SQL_MINIMAL_NODE_TYPE_SHOW_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_SET_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_KILL_CLIENT_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_RELOAD_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_PAUSE_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_RESUME_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_DROP_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_BEGIN_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_DISCARD_STMT,
} od_sql_minimal_node_tag_t;

typedef struct od_sql_minimal_node {
	od_sql_minimal_node_tag_t type;
} od_sql_minimal_node_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
	char *name;
} od_sql_minimal_show_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
	char *key;
	char *value;
} od_sql_minimal_set_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
	char *id;
} od_sql_minimal_kill_client_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
} od_sql_minimal_reload_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
} od_sql_minimal_pause_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
} od_sql_minimal_resume_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
	char *name;
} od_sql_minimal_drop_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
} od_sql_minimal_begin_stmt_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
	char *name;
	int is_all;
} od_sql_minimal_deallocate_stmt_t;

typedef enum {
	OD_SQL_MINIMAL_DISCARD_ALL,
	OD_SQL_MINIMAL_DISCARD_TEMP,
	OD_SQL_MINIMAL_DISCARD_PLANS,
	OD_SQL_MINIMAL_DISCARD_SEQUENCES,
} od_sql_minimal_discard_target_t;

typedef struct {
	od_sql_minimal_node_tag_t type;
	od_sql_minimal_discard_target_t target;
} od_sql_minimal_discard_stmt_t;

od_sql_minimal_node_t *od_sql_minimal_node_alloc(od_linear_alloc_t *al,
						 od_sql_minimal_node_tag_t type,
						 size_t size);
void od_sql_minimal_node_free(od_sql_minimal_node_t *node);

int od_sql_minimal_node_print(const od_sql_minimal_node_t *node, char *buf,
			      size_t buflen);

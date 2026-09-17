#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Console command parser AST definitions.
 */

#include <alloc/linear.h>
#include <id.h>

typedef enum {
	OD_CONSOLE_NODE_TYPE_INVALID = 0,
	OD_CONSOLE_NODE_TYPE_SHOW_STMT,
	OD_CONSOLE_NODE_TYPE_KILL_CLIENT_STMT,
	OD_CONSOLE_NODE_TYPE_RELOAD_STMT,
	OD_CONSOLE_NODE_TYPE_PAUSE_STMT,
	OD_CONSOLE_NODE_TYPE_RESUME_STMT,
	OD_CONSOLE_NODE_TYPE_SET_STMT,
	OD_CONSOLE_NODE_TYPE_DROP_STMT,
	OD_CONSOLE_NODE_TYPE_GC_STMT,
} od_console_node_tag_t;

typedef struct od_console_node {
	od_console_node_tag_t type;
} od_console_node_t;

typedef struct {
	od_console_node_tag_t type;
	char *name;
	char *arg;
} od_console_show_stmt_t;

typedef struct {
	od_console_node_tag_t type;
	char *id;
} od_console_kill_client_stmt_t;

typedef struct {
	od_console_node_tag_t type;
} od_console_reload_stmt_t;

typedef struct {
	od_console_node_tag_t type;
} od_console_pause_stmt_t;

typedef struct {
	od_console_node_tag_t type;
} od_console_resume_stmt_t;

typedef struct {
	od_console_node_tag_t type;
	char *key;
	char *value;
} od_console_set_stmt_t;

typedef enum {
	OD_CONSOLE_DROP_SERVERS = 0,
} od_console_drop_target_t;

typedef struct {
	od_console_node_tag_t type;
	od_console_drop_target_t target;
	char *path;
} od_console_drop_stmt_t;

typedef struct {
	od_console_node_tag_t type;
} od_console_gc_stmt_t;

od_console_node_t *od_console_node_alloc(od_linear_alloc_t *al,
					 od_console_node_tag_t type,
					 size_t size);
void od_console_node_free(od_console_node_t *node);
int od_console_node_print(const od_console_node_t *node, char *buf,
			  size_t buflen);

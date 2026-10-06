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
	OD_SQL_MINIMAL_NODE_TYPE_BEGIN_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_DISCARD_STMT,
	OD_SQL_MINIMAL_NODE_TYPE_UNLISTEN_STMT,
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

typedef struct {
	od_sql_minimal_node_tag_t type;
	char *name;
	int is_all;
} od_sql_minimal_unlisten_stmt_t;

#define OD_PG_NAMEDATALEN 64

typedef enum {
	OD_QUERY_CTX_PARSE_ERROR = 1 << 0,
	OD_QUERY_CTX_IS_SELECT = 1 << 1,
	OD_QUERY_CTX_IS_SHOW = 1 << 2,
	OD_QUERY_CTX_IS_SET = 1 << 3,
	OD_QUERY_CTX_IS_BEGIN = 1 << 4,
	OD_QUERY_CTX_IS_DISCARD_ALL = 1 << 5,
	OD_QUERY_CTX_IS_UNLISTEN_ALL = 1 << 6,
	OD_QUERY_CTX_IS_DEALLOCATE_ALL = 1 << 7,
	OD_QUERY_CTX_HAS_DEALLOCATE_NAME = 1 << 8,
	OD_QUERY_CTX_TOO_LONG = 1 << 9,
} od_query_ctx_flags_t;

typedef struct {
	uint64_t flags;
	/*
	 *   OD_QUERY_CTX_IS_SHOW               s1 = guc name
	 *   OD_QUERY_CTX_IS_SET                s1 = guc key, s2 = guc value
	 *   OD_QUERY_CTX_HAS_DEALLOCATE_NAME   s1 = prepared statement name
	 */
	char s1[OD_PG_NAMEDATALEN];
	char s2[OD_PG_NAMEDATALEN];
	char *s2_long;
} od_query_ctx_t;

#define od_query_ctx_has(ctx, flag) (((ctx)->flags & (flag)) != 0)
#define od_query_ctx_set(ctx, flag) ((ctx)->flags |= (flag))
#define od_query_ctx_clear(ctx, flag) ((ctx)->flags &= ~(uint64_t)(flag))

static inline int od_query_ctx_standby_friendly(const od_query_ctx_t *ctx)
{
	if (od_query_ctx_has(ctx, OD_QUERY_CTX_PARSE_ERROR |
					  OD_QUERY_CTX_TOO_LONG)) {
		return 0;
	}

	return od_query_ctx_has(ctx, OD_QUERY_CTX_IS_SELECT);
}

od_sql_minimal_node_t *od_sql_minimal_node_alloc(od_linear_alloc_t *al,
						 od_sql_minimal_node_tag_t type,
						 size_t size);
void od_sql_minimal_node_free(od_sql_minimal_node_t *node);

int od_sql_minimal_node_print(const od_sql_minimal_node_t *node, char *buf,
			      size_t buflen);

/*
 * Extract query-context flags from a minimal-parser AST into ctx.
 * On a non-matching AST, all flags are set to 0.
 * deallocate_name (if has_deallocate_name) is stored inplace (no heap alloc).
 */
void od_sql_minimal_extract_query_ctx(const od_sql_minimal_node_t *ast,
				      od_query_ctx_t *ctx);

void od_query_ctx_release(od_query_ctx_t *ctx);
void od_query_ctx_reset(od_query_ctx_t *ctx);

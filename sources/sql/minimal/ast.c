/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Minimal SQL parser AST helpers.
 */

#include <odyssey.h>
#include <stdio.h>
#include <sql/minimal/ast.h>
#include <util.h>

od_sql_minimal_node_t *od_sql_minimal_node_alloc(od_linear_alloc_t *al,
						 od_sql_minimal_node_tag_t type,
						 size_t size)
{
	od_sql_minimal_node_t *n =
		od_linear_alloc_alloc(al, size, OD_LINEAR_ALLOC_ZERO);
	if (n == NULL) {
		return NULL;
	}
	n->type = type;
	return n;
}

void od_sql_minimal_node_free(od_sql_minimal_node_t *node)
{
	/* nodes are arena-allocated; freeing is done by od_linear_alloc_reset */
	(void)node;
}

int od_sql_minimal_node_print(const od_sql_minimal_node_t *node, char *buf,
			      size_t buflen)
{
	if (node == NULL) {
		return snprintf(buf, buflen, "(null)");
	}

	switch (node->type) {
	case OD_SQL_MINIMAL_NODE_TYPE_SHOW_STMT: {
		const od_sql_minimal_show_stmt_t *n =
			(const od_sql_minimal_show_stmt_t *)node;
		return snprintf(buf, buflen, "(show %s)",
				n->name ? n->name : "");
	}

	case OD_SQL_MINIMAL_NODE_TYPE_SET_STMT: {
		const od_sql_minimal_set_stmt_t *n =
			(const od_sql_minimal_set_stmt_t *)node;
		if (n->value) {
			return snprintf(buf, buflen, "(set %s=%s)",
					n->key ? n->key : "", n->value);
		}
		return snprintf(buf, buflen, "(set %s=default)",
				n->key ? n->key : "");
	}

	case OD_SQL_MINIMAL_NODE_TYPE_BEGIN_STMT:
		return snprintf(buf, buflen, "(begin)");

	case OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT: {
		const od_sql_minimal_deallocate_stmt_t *n =
			(const od_sql_minimal_deallocate_stmt_t *)node;
		if (n->is_all) {
			return snprintf(buf, buflen, "(deallocate all)");
		}
		return snprintf(buf, buflen, "(deallocate %s)",
				n->name ? n->name : "");
	}

	case OD_SQL_MINIMAL_NODE_TYPE_UNLISTEN_STMT: {
		const od_sql_minimal_unlisten_stmt_t *n =
			(const od_sql_minimal_unlisten_stmt_t *)node;
		if (n->is_all) {
			return snprintf(buf, buflen, "(unlisten *)");
		}
		return snprintf(buf, buflen, "(unlisten %s)",
				n->name ? n->name : "");
	}

	case OD_SQL_MINIMAL_NODE_TYPE_DISCARD_STMT: {
		const od_sql_minimal_discard_stmt_t *n =
			(const od_sql_minimal_discard_stmt_t *)node;
		switch (n->target) {
		case OD_SQL_MINIMAL_DISCARD_ALL:
			return snprintf(buf, buflen, "(discard all)");
		case OD_SQL_MINIMAL_DISCARD_TEMP:
			return snprintf(buf, buflen, "(discard temp)");
		case OD_SQL_MINIMAL_DISCARD_PLANS:
			return snprintf(buf, buflen, "(discard plans)");
		case OD_SQL_MINIMAL_DISCARD_SEQUENCES:
			return snprintf(buf, buflen, "(discard sequences)");
		}
		return snprintf(buf, buflen, "(discard unknown)");
	}

	default:
		return snprintf(buf, buflen, "(unknown:%d)", (int)node->type);
	}
}

void od_sql_minimal_extract_query_ctx(const od_sql_minimal_node_t *ast,
				      od_query_ctx_t *ctx)
{
	od_query_ctx_release(ctx);

	ctx->flags = 0;
	ctx->s1[0] = '\0';
	ctx->s2[0] = '\0';
	ctx->s2_long = NULL;

	if (ast == NULL) {
		od_query_ctx_set(ctx, OD_QUERY_CTX_PARSE_ERROR);
		return;
	}

	switch (ast->type) {
	case OD_SQL_MINIMAL_NODE_TYPE_SHOW_STMT: {
		const od_sql_minimal_show_stmt_t *n =
			(const od_sql_minimal_show_stmt_t *)ast;
		od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SHOW);
		od_snprintf(ctx->s1, sizeof(ctx->s1), "%s", n->name);
		break;
	}
	case OD_SQL_MINIMAL_NODE_TYPE_SET_STMT: {
		const od_sql_minimal_set_stmt_t *n =
			(const od_sql_minimal_set_stmt_t *)ast;
		od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SET);
		od_snprintf(ctx->s1, sizeof(ctx->s1), "%s", n->key);

		const char *value = n->value != NULL ? n->value : "";
		od_snprintf(ctx->s2, sizeof(ctx->s2), "%s", value);
		size_t len = strlen(value);
		if (len >= sizeof(ctx->s2)) {
			ctx->s2_long = od_malloc(len + 1);
			if (ctx->s2_long != NULL) {
				memcpy(ctx->s2_long, value, len + 1);
			}
		}
		break;
	}
	case OD_SQL_MINIMAL_NODE_TYPE_BEGIN_STMT:
		od_query_ctx_set(ctx, OD_QUERY_CTX_IS_BEGIN);
		break;
	case OD_SQL_MINIMAL_NODE_TYPE_UNLISTEN_STMT: {
		const od_sql_minimal_unlisten_stmt_t *n =
			(const od_sql_minimal_unlisten_stmt_t *)ast;
		if (n->is_all) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_UNLISTEN_ALL);
		}
		break;
	}
	case OD_SQL_MINIMAL_NODE_TYPE_DISCARD_STMT: {
		const od_sql_minimal_discard_stmt_t *n =
			(const od_sql_minimal_discard_stmt_t *)ast;
		if (n->target == OD_SQL_MINIMAL_DISCARD_ALL) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_DISCARD_ALL);
		}
		break;
	}
	case OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT: {
		const od_sql_minimal_deallocate_stmt_t *n =
			(const od_sql_minimal_deallocate_stmt_t *)ast;
		if (n->is_all) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_DEALLOCATE_ALL);
		} else if (n->name != NULL) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_HAS_DEALLOCATE_NAME);
			od_snprintf(ctx->s1, sizeof(ctx->s1), "%s", n->name);
		}
		break;
	}
	default:
		break;
	}
}

void od_query_ctx_release(od_query_ctx_t *ctx)
{
	if (ctx->s2_long != NULL) {
		od_free(ctx->s2_long);
		ctx->s2_long = NULL;
	}
}

void od_query_ctx_reset(od_query_ctx_t *ctx)
{
	od_query_ctx_release(ctx);
	memset(ctx, 0, sizeof(od_query_ctx_t));
}

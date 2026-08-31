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

od_sql_minimal_node_t *od_sql_minimal_node_alloc(od_linear_alloc_t *al,
						 od_sql_minimal_node_tag_t type,
						 size_t size)
{
	od_sql_minimal_node_t *n = od_linear_alloc_alloc(al, size);
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

	case OD_SQL_MINIMAL_NODE_TYPE_KILL_CLIENT_STMT: {
		const od_sql_minimal_kill_client_stmt_t *n =
			(const od_sql_minimal_kill_client_stmt_t *)node;
		return snprintf(buf, buflen, "(kill-client %s)", n->id);
	}

	case OD_SQL_MINIMAL_NODE_TYPE_RELOAD_STMT:
		return snprintf(buf, buflen, "(reload)");

	case OD_SQL_MINIMAL_NODE_TYPE_PAUSE_STMT:
		return snprintf(buf, buflen, "(pause)");

	case OD_SQL_MINIMAL_NODE_TYPE_RESUME_STMT:
		return snprintf(buf, buflen, "(resume)");

	case OD_SQL_MINIMAL_NODE_TYPE_DROP_STMT: {
		const od_sql_minimal_drop_stmt_t *n =
			(const od_sql_minimal_drop_stmt_t *)node;
		return snprintf(buf, buflen, "(drop %s)",
				n->name ? n->name : "");
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

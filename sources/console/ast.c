/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Console command parser AST helpers.
 */

#include <odyssey.h>
#include <stdio.h>
#include <console/ast.h>
#include <util.h>

od_console_node_t *od_console_node_alloc(od_linear_alloc_t *al,
					 od_console_node_tag_t type,
					 size_t size)
{
	od_console_node_t *n = od_linear_alloc_alloc(al, size);
	if (n == NULL) {
		return NULL;
	}
	n->type = type;
	return n;
}

void od_console_node_free(od_console_node_t *node)
{
	(void)node;
}

int od_console_node_print(const od_console_node_t *node, char *buf,
			  size_t buflen)
{
	if (node == NULL) {
		return snprintf(buf, buflen, "(null)");
	}

	switch (node->type) {
	case OD_CONSOLE_NODE_TYPE_SHOW_STMT: {
		const od_console_show_stmt_t *n =
			(const od_console_show_stmt_t *)node;
		if (n->arg) {
			return snprintf(buf, buflen, "(show %s %s)",
					n->name ? n->name : "", n->arg);
		}
		return snprintf(buf, buflen, "(show %s)",
				n->name ? n->name : "");
	}

	case OD_CONSOLE_NODE_TYPE_KILL_CLIENT_STMT: {
		const od_console_kill_client_stmt_t *n =
			(const od_console_kill_client_stmt_t *)node;
		return snprintf(buf, buflen, "(kill-client %s)", n->id);
	}

	case OD_CONSOLE_NODE_TYPE_RELOAD_STMT:
		return snprintf(buf, buflen, "(reload)");

	case OD_CONSOLE_NODE_TYPE_PAUSE_STMT:
		return snprintf(buf, buflen, "(pause)");

	case OD_CONSOLE_NODE_TYPE_RESUME_STMT:
		return snprintf(buf, buflen, "(resume)");

	case OD_CONSOLE_NODE_TYPE_SET_STMT: {
		const od_console_set_stmt_t *n =
			(const od_console_set_stmt_t *)node;
		if (n->value) {
			return snprintf(buf, buflen, "(set %s=%s)",
					n->key ? n->key : "", n->value);
		}
		return snprintf(buf, buflen, "(set %s=default)",
				n->key ? n->key : "");
	}

	case OD_CONSOLE_NODE_TYPE_DROP_STMT: {
		const od_console_drop_stmt_t *n =
			(const od_console_drop_stmt_t *)node;
		switch (n->target) {
		case OD_CONSOLE_DROP_SERVERS:
			return snprintf(buf, buflen, "(drop servers)");
		}
		return snprintf(buf, buflen, "(drop unknown)");
	}

	case OD_CONSOLE_NODE_TYPE_GC_STMT:
		return snprintf(buf, buflen, "(gc)");

	default:
		return snprintf(buf, buflen, "(unknown:%d)", (int)node->type);
	}
}

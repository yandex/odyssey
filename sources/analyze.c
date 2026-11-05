/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>
#include <client.h>
#include <global.h>
#include <instance.h>
#include <query.h>
#include <worker.h>
#include <analyze.h>
#include <sql/minimal/parser.h>
#include <sql/full/parser.h>

static bool reserved_name(const char *name)
{
	return name != NULL && strncmp(name, "odyssey", 7) == 0;
}

/* the prepared statement a statement creates, executes or deallocates */
const char *stmt_pstmt_name(Node *node)
{
	/* EXPLAIN [ANALYZE] CREATE TABLE ... AS EXECUTE nests two levels */
	while (node != NULL &&
	       (IsA(node, ExplainStmt) || IsA(node, CreateTableAsStmt))) {
		if (IsA(node, ExplainStmt)) {
			node = castNode(ExplainStmt, node)->query;
		} else {
			node = castNode(CreateTableAsStmt, node)->query;
		}
	}

	if (node == NULL) {
		return NULL;
	}

	if (IsA(node, PrepareStmt)) {
		return castNode(PrepareStmt, node)->name;
	}
	if (IsA(node, ExecuteStmt)) {
		return castNode(ExecuteStmt, node)->name;
	}
	if (IsA(node, DeallocateStmt)) {
		return castNode(DeallocateStmt, node)->name;
	}

	return NULL;
}

static bool sql_reserved(od_client_t *client, const char *query, size_t len)
{
	od_instance_t *instance = client->global->instance;
	od_config_query_parsing_mode_t mode = od_query_parser_mode(client);
	if (!client->rule->pool->reserve_prepared_statement ||
	    mode == OD_CONFIG_QUERY_PARSING_MODE_DISABLED ||
	    len > instance->config.query_parsing.max_query_len) {
		return false;
	}

	/*XXX: support u&-style names later*/
	if (strcasestr(query, "odyssey") == NULL) {
		return false;
	}
	od_linear_alloc_t *arena = od_worker_get_local_linear_alloc();
	if (arena == NULL || arena->buf == NULL) {
		return false;
	}

	if (mode == OD_CONFIG_QUERY_PARSING_MODE_MINIMAL) {
		od_sql_minimal_node_t *node =
			od_sql_minimal_parse(query, len, arena, NULL, NULL);
		if (node == NULL) {
			return false;
		}
		switch (node->type) {
		case OD_SQL_MINIMAL_NODE_TYPE_PREPARE_STMT:
			return reserved_name(
				((od_sql_minimal_prepare_stmt_t *)node)->name);
		case OD_SQL_MINIMAL_NODE_TYPE_EXECUTE_STMT:
			return reserved_name(
				((od_sql_minimal_execute_stmt_t *)node)->name);
		case OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT:
			return reserved_name(
				((od_sql_minimal_deallocate_stmt_t *)node)
					->name);
		default:
			return false;
		}
	}

	List *tree = od_sql_full_parse(arena, query, len, NULL, NULL);
	ListCell *cell;
	foreach(cell, tree)
	{
		if (reserved_name(stmt_pstmt_name(
			    lfirst_node(RawStmt, cell)->stmt))) {
			return true;
		}
	}
	return false;
}

od_frontend_status_t od_sql_analyze(od_client_t *client, const char *query,
				    size_t len, machine_msg_t **error)
{
	*error = NULL;
	char *code;
	char *message;
	if (sql_reserved(client, query, len)) {
		code = KIWI_RESERVED_NAME;
		message =
			"prepared statement names starting with \"odyssey\" are reserved by odyssey";
	} else if (client->sql_query.unsupported) {
		code = KIWI_FEATURE_NOT_SUPPORTED;
		message =
			"SQL prepared statement syntax is not supported with pool_reserve_prepared_statement";
	} else {
		return OD_OK;
	}
	*error = kiwi_be_write_error(NULL, code, message, strlen(message));
	return *error != NULL ? OD_SKIP : OD_EOOM;
}

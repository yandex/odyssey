/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>
#include <client.h>
#include <analyze.h>
#include <global.h>
#include <instance.h>
#include <pstmt.h>
#include <query.h>
#include <worker.h>
#include <sql/full/parser.h>

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

static const od_query_ctx_t *od_sql_parse(od_client_t *client,
					  const char *query, uint32_t query_len,
					  const od_pstmt_t *pstmt,
					  od_query_ctx_t *parsed_ctx)
{
	od_instance_t *instance = client->global->instance;
	od_config_query_parsing_t parsing = instance->config.query_parsing;
	parsing.mode = od_query_parser_mode(client);
	od_linear_alloc_t *arena = od_worker_get_local_linear_alloc();
	if (arena == NULL || arena->buf == NULL) {
		/*
		 * For now, forward the query without local parsing or analysis
		 * if the worker's linear allocator is unavailable.
		 */
		od_query_ctx_reset(parsed_ctx);
		return NULL;
	}

	if (pstmt != NULL && pstmt->query_parser_mode == parsing.mode) {
		return &pstmt->query_ctx;
	}

	/* Parse Query text, or use the client's grammar for a shared statement. */
	od_query_parse_fill_ctx(query, query_len, arena, parsed_ctx, &parsing);
	return parsed_ctx;
}

const od_query_ctx_t *od_sql_parse_for_analysis(od_client_t *client,
						const char *query,
						uint32_t query_len,
						const od_pstmt_t *pstmt,
						od_query_ctx_t *parsed_ctx)
{
	od_instance_t *instance = client->global->instance;
	if (query_len > instance->config.query_parsing.max_query_len ||
	    od_query_parser_mode(client) ==
		    OD_CONFIG_QUERY_PARSING_MODE_DISABLED) {
		od_query_ctx_reset(parsed_ctx);
		return NULL;
	}

	const od_query_ctx_t *ctx =
		od_sql_parse(client, query, query_len, pstmt, parsed_ctx);
	if (ctx == NULL || od_query_ctx_has(ctx, OD_QUERY_CTX_PARSE_ERROR)) {
		return NULL;
	}
	return ctx;
}

od_frontend_status_t od_sql_analyze(od_client_t *client,
				    const od_query_ctx_t *ctx,
				    machine_msg_t **error)
{
	*error = NULL;
	if (!client->rule->pool->reserve_prepared_statement ||
	    !od_query_ctx_has(ctx, OD_QUERY_CTX_HAS_RESERVED_NAME)) {
		return OD_OK;
	}
	static char message[] =
		"prepared statement names starting with \"odyssey\" are reserved by odyssey";
	*error = kiwi_be_write_error(NULL, KIWI_RESERVED_NAME, message,
				     sizeof(message) - 1);
	return *error != NULL ? OD_SKIP : OD_EOOM;
}

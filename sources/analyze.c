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

static od_frontend_status_t analyze_query_ctx(od_client_t *client,
					    const od_query_ctx_t *ctx,
					    machine_msg_t **error)
{
	if (!client->rule->pool->reserve_prepared_statement ||
	    od_query_ctx_has(ctx, OD_QUERY_CTX_PARSE_ERROR) ||
	    !od_query_ctx_has(ctx, OD_QUERY_CTX_HAS_RESERVED_NAME)) {
		return OD_OK;
	}
	static char message[] =
		"prepared statement names starting with \"odyssey\" are reserved by odyssey";
	*error = kiwi_be_write_error(NULL, KIWI_RESERVED_NAME, message,
				     sizeof(message) - 1);
	return *error != NULL ? OD_SKIP : OD_EOOM;
}

od_frontend_status_t od_sql_analyze(od_client_t *client, const char *query,
				    uint32_t query_len, const od_pstmt_t *pstmt,
				    machine_msg_t **error)
{
	*error = NULL;
	od_instance_t *instance = client->global->instance;
	od_config_query_parsing_t parsing = instance->config.query_parsing;
	parsing.mode = od_query_parser_mode(client);
	od_linear_alloc_t *arena = od_worker_get_local_linear_alloc();
	if (pstmt != NULL &&
	    (arena == NULL || arena->buf == NULL ||
	     parsing.mode == OD_CONFIG_QUERY_PARSING_MODE_DISABLED ||
	     query_len > parsing.max_query_len)) {
		return OD_OK;
	}

	od_query_ctx_t temporary_ctx = { 0 };
	const od_query_ctx_t *ctx =
		pstmt != NULL ? &pstmt->query_ctx : &client->query_ctx;
	/*
	 * Query has no cached statement. Parse can reuse its first context unless
	 * another grammar or an earlier length limit requires a fresh one.
	 */
	if (pstmt == NULL || pstmt->query_parser_mode != parsing.mode ||
	    od_query_ctx_has(ctx, OD_QUERY_CTX_TOO_LONG)) {
		od_query_ctx_t *parsed_ctx =
			pstmt != NULL ? &temporary_ctx : &client->query_ctx;
		od_query_parse_fill_ctx(query, query_len, arena, parsed_ctx,
					&parsing);
		ctx = parsed_ctx;
	}
	od_frontend_status_t status = analyze_query_ctx(client, ctx, error);
	od_query_ctx_reset(&temporary_ctx);
	return status;
}

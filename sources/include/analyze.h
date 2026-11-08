#pragma once

#include <stdint.h>

#include <types.h>
#include <status.h>
#include <machinarium/msg.h>
#include <sql/minimal/ast.h>

struct Node;

const char *stmt_pstmt_name(struct Node *node);

/*
 * Reuse the statement's context when its grammar matches the client.
 * Otherwise parse into parsed_ctx, which the caller initializes and resets.
 * NULL pstmt represents a simple Query. Return NULL if parsing is disabled,
 * the query exceeds the length limit, the arena is unavailable or parsing fails.
 */
const od_query_ctx_t *od_sql_parse_for_analysis(od_client_t *client,
						const char *query,
						uint32_t query_len,
						const od_pstmt_t *pstmt,
						od_query_ctx_t *parsed_ctx);

/*
 * Call only for a successfully parsed query within the client's length limit.
 * OD_SKIP returns an ErrorResponse in *error, owned by the caller.
 */
od_frontend_status_t od_sql_analyze(od_client_t *client,
				    const od_query_ctx_t *ctx,
				    machine_msg_t **error);

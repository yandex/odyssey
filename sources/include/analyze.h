#pragma once

#include <stdint.h>

#include <types.h>
#include <status.h>
#include <machinarium/msg.h>

struct Node;

const char *stmt_pstmt_name(struct Node *node);

/*
 * Reuse the statement's context when its grammar matches the client.
 * NULL pstmt represents a simple Query: parse into client->query_ctx.
 * OD_SKIP returns an ErrorResponse in *error, owned by the caller.
 */
od_frontend_status_t od_sql_analyze(od_client_t *client, const char *query,
				    uint32_t query_len, const od_pstmt_t *pstmt,
				    machine_msg_t **error);

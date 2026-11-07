#pragma once

#include <types.h>
#include <status.h>
#include <machinarium/msg.h>

struct Node;

const char *stmt_pstmt_name(struct Node *node);

/* OD_SKIP returns an ErrorResponse in *error, owned by the caller. */
od_frontend_status_t od_sql_analyze(od_client_t *client, const char *query,
				    size_t len, machine_msg_t **error);

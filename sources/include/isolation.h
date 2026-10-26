#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>
#include <status.h>
#include <types.h>

/* Recognize the native isolationtester's prepared lock probe using its AST. */
void od_isolation_probe_parse(od_pstmt_t *pstmt, od_linear_alloc_t *arena);

/* Returned statement/message are owned by the caller; NULL means no rewrite. */
od_frontend_status_t od_isolation_probe_bind(od_client_t *client,
					     const od_pstmt_t *pstmt,
					     machine_msg_t *bind,
					     od_pstmt_t **rewritten_pstmt,
					     machine_msg_t **rewritten_bind);

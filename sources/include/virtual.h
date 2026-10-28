#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * Virtual portals: the query is answered by odyssey itself, without the
 * backend (see virtual.c). Everything below try_virtual_process_query()
 * only composes the portal and never touches the client io.
 */

#include <odyssey.h>

#include <machinarium/msg.h>

#include <status.h>
#include <query.h>
#include <tuple_table_slot.h>

typedef struct {
	/* tuples for SHOW-like answers; NULL when the answer has none */
	od_tuple_table_slot_t *tts;

	/*
	 * the portal payload as already assembled protocol messages,
	 * in the wire order; any of them may be absent:
	 *
	 *   NOTICE "processed virtually by odyssey"
	 *   RowDescription + DataRows   (SHOW-like answers)
	 *   CommandComplete            (or ErrorResponse on a reject)
	 *   ReadyForQuery              (assembled on the write path,
	 *                               as the tx status is live)
	 */
	machine_msg_t *notice;
	machine_msg_t *command_complete;
	machine_msg_t *error;

	/*
	 * ReadyForQuery transaction status; when zero, it is taken from
	 * the server the client is attached to (BEGIN forces its own)
	 */
	uint8_t txstatus_force;
} od_virtual_portal_t;

void virtual_portal_init(od_virtual_portal_t *portal);
void virtual_portal_free(od_virtual_portal_t *portal);

od_frontend_status_t try_virtual_process_query(od_client_t *client,
					       const od_query_ctx_t *ctx,
					       bool uses_reserved_name);

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <status.h>
#include <types.h>

#include <machinarium/machinarium.h>
#include <machinarium/ds/vector.h>

typedef enum {
	/*
	 * nothing changed on entry execution
	 */
	OD_XPLAN_DELTA_NONE,

	/*
	 * add pstmt to both client and server hashmaps
	 * in case Parse for stmt which is not exists on server
	 */
	OD_XPLAN_DELTA_ADD_BOTH,

	/*
	 * add pstmt only to client's hashmap
	 * in case pstmt already existed on server
	 */
	OD_XPLAN_DELTA_ADD_CLIENT_ONLY,

	/*
	 * add pstmt only to server's hashmap
	 * for cases of shadow parsing of stmt, that is not
	 * exists on server
	 */
	OD_XPLAN_DELTA_ADD_SERVER_ONLY,

	/*
	 * remove pstmt from client map, on Close
	 */
	OD_XPLAN_DELTA_REMOVE_CLIENT_ONLY,

	/*
	 * remove all pstmts from both client and server hashmaps
	 * in case of Execute(DISCARD ALL)
	 */
	OD_XPLAN_DELTA_REMOVE_ALL
} od_xplan_delta_type_t;

typedef struct {
	od_xplan_delta_type_t type;
	const char *client_pstmt;
	const od_pstmt_t *pstmt;
} od_xplan_delta_t;

typedef enum {
	/*
	 * send to backend and proxy the answer to client
	 * this is:
	 * - simple proxy without rewriting the msg: Execute, Flush, Sync, Describe(P)
	 * - rewritten: Describe(S), Bind
	 */
	OD_XPLAN_FORWARD,

	/*
	 * Parse with rewritten stmt name,
	 * which must be added to client and server hashmaps
	 * on success PC
	 */
	OD_XPLAN_PARSE,

	/*
	 * inserted Parse, which PC must not be sent to client
	 */
	OD_XPLAN_PARSE_SHADOW,

	/*
	 * CloseComplete for stmt on client
	 */
	OD_XPLAN_VIRTUAL_CLOSE_COMPLETE,

	/*
	 * ParseComplete for stmt, that already exists on backend
	 */
	OD_XPLAN_VIRTUAL_PARSE_COMPLETE,

	/*
	 * virtual ErrorResponse for unknown or already exists statements
	 */
	OD_XPLAN_VIRTUAL_ERROR_RESPONSE,
} od_xplan_entry_type_t;

typedef struct {
	od_xplan_entry_type_t type;

	/*
	 * applied after receiving corresponding success response
	 * from backend
	 * 
	 * ex: applied with adding new pstmts after receiving
	 * corresponding ParseComplete
	 */
	od_xplan_delta_t delta;

	/*
	 * original msg from client
	 * can be sent to server, if it wan't rewritten to server_msg
	 *
	 * cannot be NULL, but is owned by xbuf
	 */
	machine_msg_t *clmsg;

	/*
	 * rewritten client msg to send to server
	 *
	 * can be NULL, owned by xplan
	 */
	machine_msg_t *srvmsg;

	/*
	 * virtual response to client
	 * note:
	 * no ParseComplete msg here - it is always const seq of bytes
	 * no CloseComplete msg here - it is always const seq of bytes
	 *
	 * can be NULL, owned by xplan
	 */
	machine_msg_t *vresp;
} od_xplan_entry_t;

struct od_xplan {
	/* od_xplan_entry_t[] */
	mm_vector_t entries;
};

void od_xplan_init(od_xplan_t *xp);
void od_xplan_destroy(od_xplan_t *xp);
void od_xplan_clear(od_xplan_t *xp);

od_frontend_status_t od_xplan_make_from_xbuf(od_xplan_t *xp, od_relay_t *relay);

od_frontend_status_t od_xplan_run(od_xplan_t *xp, od_relay_t *relay,
				  uint32_t timeout_ms);

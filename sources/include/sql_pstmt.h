#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * SQL PREPARE support for reserved prepared statements
 *
 * The text of every successful SQL PREPARE is kept per client. Whenever the
 * client is attached to a backend, the backend is made to hold exactly the
 * client's statements under the same names: statements left by previous
 * clients are deallocated (so no client can see or execute another client's
 * statements) and missing ones are prepared again. EXECUTE is passed through
 * unchanged. PREPARE is not transactional in PostgreSQL, so a statement is
 * recorded on its CommandComplete regardless of how the transaction ends.
 *
 * state, kept in od_sql_pstmt_client_t and od_sql_pstmt_server_t:
 * 	client: name -> PREPARE text and its hash
 * 	server: name -> hash of the PREPARE text the backend holds
 */

#include <stdint.h>
#include <stddef.h>

#include <machinarium/ds/hm.h>

#include <types.h>
#include <list.h>

/* forward decls */
struct od_global;
struct od_pstmt;

typedef struct {
	/* insertion order, the oldest first */
	od_list_t link;
	char *name;
	/* NUL-terminated PREPARE statement; NULL in server maps */
	char *text;
	/* length of text without the terminating NUL */
	size_t text_len;
	uint64_t hash;
	/*
	 * PREPARE statements are mirrored in the global prepared statements
	 * map (shown by SHOW GLOBAL_PREPARED_STATEMENTS); a client-side
	 * entry holds a reference, NULL in server maps
	 */
	struct od_pstmt *pstmt;
} od_sql_pstmt_t;

typedef struct {
	/* name -> od_sql_pstmt_t *, created on first insert */
	mm_hashmap_t *hm;
	od_list_t order;
	size_t count;
} od_sql_pstmt_map_t;

typedef enum {
	OD_SQL_PSTMT_OP_PREPARE,
	OD_SQL_PSTMT_OP_DEALLOCATE,
	OD_SQL_PSTMT_OP_DEALLOCATE_ALL,
} od_sql_pstmt_op_type_t;

/* PREPARE or DEALLOCATE statement found in a client query */
typedef struct {
	od_sql_pstmt_op_type_t type;
	/* position of the statement in the query, starting from 0 */
	int stmt_idx;
	/* NULL for DEALLOCATE ALL */
	char *name;
	/* PREPARE only */
	char *text;
	/* length of text without the terminating NUL */
	size_t text_len;
	/*
	 * PREPARE only: the prepared statement itself, without the
	 * "PREPARE name AS" prologue, points into text
	 */
	char *body;
	size_t body_len;
	uint64_t hash;
} od_sql_pstmt_op_t;

/* state of the simple query being processed */
typedef struct {
	/* the query was already inspected (the handler can run twice) */
	int analyzed;
	/* count CommandComplete messages of the query */
	int counting;
	int completes;
	od_sql_pstmt_op_t *ops;
	int nops;
	/*
	 * a statement name in the query starts with OD_PSTMT_NAME_PREFIX,
	 * which odyssey uses for its own prepared statements on backends
	 */
	int uses_reserved_name;
} od_sql_pstmt_query_t;

/* per client: its statements and the simple query being processed */
typedef struct {
	od_sql_pstmt_map_t stmts;
	od_sql_pstmt_query_t query;
} od_sql_pstmt_client_t;

/* per server connection */
typedef struct {
	od_sql_pstmt_map_t stmts;
	/*
	 * the backend may hold statements created by SQL PREPARE that are not
	 * in stmts (recording one failed): it must not be reused
	 */
	int unknown;
} od_sql_pstmt_server_t;

/* maps */
void od_sql_pstmt_map_init(od_sql_pstmt_map_t *map);
void od_sql_pstmt_map_free(od_sql_pstmt_map_t *map);
void od_sql_pstmt_map_clear(od_sql_pstmt_map_t *map);
od_sql_pstmt_t *od_sql_pstmt_map_get(od_sql_pstmt_map_t *map, const char *name);
/* adds or replaces the entry; text may be NULL */
od_sql_pstmt_t *od_sql_pstmt_map_set(od_sql_pstmt_map_t *map, const char *name,
				     const char *text, size_t text_len,
				     uint64_t hash);
void od_sql_pstmt_map_remove(od_sql_pstmt_map_t *map, const char *name);

/*
 * query inspection
 *
 * od_sql_pstmt_query_analyze finds PREPARE and DEALLOCATE statements in the
 * query text (query must be NUL-terminated at query[len]) and detects
 * prepared statement names reserved by odyssey. Returns -1 on out of
 * memory, otherwise 0; queries that can't be parsed are not tracked.
 */
int od_sql_pstmt_query_analyze(od_sql_pstmt_query_t *q, const char *query,
			       size_t len, od_linear_alloc_t *arena);
void od_sql_pstmt_query_reset(od_sql_pstmt_query_t *q);
void od_sql_pstmt_op_free(od_sql_pstmt_op_t *op);

void od_sql_pstmt_client_init(od_sql_pstmt_client_t *client);
void od_sql_pstmt_client_free(od_sql_pstmt_client_t *client);
void od_sql_pstmt_server_init(od_sql_pstmt_server_t *server);
void od_sql_pstmt_server_free(od_sql_pstmt_server_t *server);

/* the client or backend dropped all its prepared statements */
void od_sql_pstmt_client_clear(od_sql_pstmt_client_t *client);
void od_sql_pstmt_server_clear(od_sql_pstmt_server_t *server);

/*
 * the statement must reach the backend instead of being answered by
 * odyssey: DEALLOCATE of a statement created by SQL PREPARE
 */
int od_sql_pstmt_op_must_forward(od_sql_pstmt_client_t *client,
				 const od_sql_pstmt_op_t *op);
int od_sql_pstmt_query_must_forward(od_sql_pstmt_client_t *client);

static inline void od_sql_pstmt_query_start_counting(od_sql_pstmt_query_t *q)
{
	if (q->nops > 0) {
		q->completes = 0;
		q->counting = 1;
	}
}

static inline void od_sql_pstmt_query_on_complete(od_sql_pstmt_query_t *q)
{
	if (q->counting) {
		q->completes++;
	}
}

/*
 * the backend completed the statement: update client and server state
 * (server may be NULL). Returns -1 on out of memory; if a completed
 * PREPARE could not be recorded, server->unknown is set.
 *
 * global may be NULL; when set, a successfully recorded PREPARE is also
 * mirrored in the global prepared statements map with a reference held
 * by the client entry (so SHOW GLOBAL_PREPARED_STATEMENTS sees it).
 *
 * DEALLOCATE ALL drops protocol-level prepared statements too; the caller
 * must forget those itself.
 */
int od_sql_pstmt_apply(od_sql_pstmt_client_t *client,
		       od_sql_pstmt_server_t *server,
		       const od_sql_pstmt_op_t *op, struct od_global *global);

/*
 * after a simple query: apply the statements the backend completed.
 * Sets *deallocated_all if a DEALLOCATE ALL completed (see
 * od_sql_pstmt_apply). Returns -1 on out of memory.
 */
int od_sql_pstmt_query_finish(od_sql_pstmt_client_t *client,
			      od_sql_pstmt_server_t *server,
			      int *deallocated_all, struct od_global *global);

/*
 * on attach of a client to a server connection: make the backend hold
 * exactly the statements of the client.
 *
 * Returns 0 on success, -1 on io error, 1 when the backend holds prepared
 * statements odyssey does not know about (srv->unknown is set, or a
 * DEALLOCATE failed for another reason than a missing statement, or a
 * name to prepare is already taken): the caller must not use this backend.
 */
int od_sql_pstmt_attach(od_sql_pstmt_client_t *client,
			od_sql_pstmt_server_t *srv, od_server_t *server,
			char *context, uint32_t timeout_ms);

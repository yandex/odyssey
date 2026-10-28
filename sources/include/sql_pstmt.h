#pragma once

/* SQL PREPARE/EXECUTE use the same client and backend maps as Parse/Bind. */
#include <pstmt.h>
#include <status.h>
#include <sql/minimal/ast.h>

typedef enum {
	OD_SQL_PSTMT_OTHER,
	OD_SQL_PSTMT_PREPARE,
	OD_SQL_PSTMT_EXECUTE,
	OD_SQL_PSTMT_DEALLOCATE,
	OD_SQL_PSTMT_DEALLOCATE_ALL,
	OD_SQL_PSTMT_DISCARD_ALL,
} od_sql_pstmt_op_type_t;

typedef struct {
	od_sql_pstmt_op_type_t type;
	od_sql_location_t location;
	od_sql_location_t name_location;
	od_sql_location_t query_location;
	char *name;
	od_pstmt_t *pstmt;
	int completed;
	int dummy;
	int parses;
	int parse_body;
	char *query;
	int nargs;
	char **args;
	int *arg_lengths;
} od_sql_pstmt_op_t;

typedef struct {
	int analyzed;
	int active;
	int uses_reserved_name;
	int unsupported;
	int completes;
	int pending_begin;
	char *text;
	machine_msg_t *rewritten;
	od_sql_pstmt_op_t *ops;
	int nops;
} od_sql_pstmt_query_t;

int od_sql_pstmt_query_analyze(od_sql_pstmt_query_t *q, const char *query,
			       size_t len, od_linear_alloc_t *arena);
void od_sql_pstmt_query_reset(od_sql_pstmt_query_t *q);
od_frontend_status_t od_sql_pstmt_query_rewrite(od_client_t *client);
od_frontend_status_t od_sql_pstmt_query_response(od_client_t *client,
						 machine_msg_t **msg);
od_frontend_status_t od_sql_pstmt_query_finish(od_client_t *client);

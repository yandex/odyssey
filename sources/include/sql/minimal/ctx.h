#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Minimal SQL parser context.
 */

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <sql/minimal/ast.h>

typedef void (*od_sql_minimal_error_cb_t)(const char *msg, void *userdata);

typedef struct od_sql_minimal_parse_ctx {
	/* input buffer -- kept for offset arithmetic in SET_YYLLOC */
	const char *scanbuf;
	size_t scanbuflen;

	od_linear_alloc_t *arena;

	od_sql_minimal_node_t *result;

	od_sql_minimal_error_cb_t error_cb;
	void *error_cb_userdata;

	int had_error;

	/*
	 * used to recover from flex YY_FATAL_ERROR
	 * (which calls exit() by default)
	 */
	jmp_buf fatal_jmp;
	int fatal_set;
} od_sql_minimal_parse_ctx_t;

void od_sql_minimal_parse_ctx_init(od_sql_minimal_parse_ctx_t *ctx,
				   const char *input, size_t input_len,
				   od_linear_alloc_t *arena,
				   od_sql_minimal_error_cb_t error_cb,
				   void *userdata);

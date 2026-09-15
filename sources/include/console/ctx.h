#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Console command parser context.
 */

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <console/ast.h>

typedef void (*od_console_error_cb_t)(const char *msg, void *userdata);

typedef struct od_console_parse_ctx {
	const char *scanbuf;
	size_t scanbuflen;

	od_linear_alloc_t *arena;

	od_console_node_t *result;

	od_console_error_cb_t error_cb;
	void *error_cb_userdata;

	int had_error;

	jmp_buf fatal_jmp;
	int fatal_set;
} od_console_parse_ctx_t;

void od_console_parse_ctx_init(od_console_parse_ctx_t *ctx, const char *input,
			       size_t input_len, od_linear_alloc_t *arena,
			       od_console_error_cb_t error_cb, void *userdata);

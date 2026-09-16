#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Console command parser public API.
 */

#include <stddef.h>
#include <console/ast.h>
#include <alloc/linear.h>
#include <console/ctx.h>

/*
 * returns the AST root on success, NULL on parse error.
 */
od_console_node_t *od_console_parse(const char *input, size_t input_len,
				    od_linear_alloc_t *arena,
				    od_console_error_cb_t error_cb,
				    void *userdata);

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>
#include <sql/minimal/ast.h>
#include <alloc/linear.h>
#include <sql/minimal/ctx.h>

/*
 * returns the AST root on success, NULL on parse error.
 */
od_sql_minimal_node_t *od_sql_minimal_parse(const char *input, size_t input_len,
					    od_linear_alloc_t *arena,
					    od_sql_minimal_error_cb_t error_cb,
					    void *userdata);

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
 * PREPARE/EXECUTE preserve opaque SQL ranges; use the full parser to
 * validate their bodies, type lists and arguments. Statement names retain
 * the existing lexer subset: ASCII unquoted names or plain quoted names
 * with at most 63 raw bytes inside quotes. Doubled quotes, U&-escaped names
 * and nested comments in statement headers are not supported.
 */
od_sql_minimal_node_t *od_sql_minimal_parse(const char *input, size_t input_len,
					    od_linear_alloc_t *arena,
					    od_sql_minimal_error_cb_t error_cb,
					    void *userdata);

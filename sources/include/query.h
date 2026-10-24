#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi/kiwi.h>
#include <machinarium/machinarium.h>

#include <types.h>
#include <config.h>
#include <server.h>
#include <sql/minimal/ast.h>

/* On success, result is the single DataRow, or NULL if no rows were returned. */
int od_query_do(od_server_t *server, char *context, const char *query,
		char *param, uint32_t timeout_ms, machine_msg_t **result);

void od_query_parse_fill_ctx(const char *query, uint32_t query_len,
			     od_linear_alloc_t *arena, od_query_ctx_t *ctx,
			     const od_config_query_parsing_t *parsing);

__attribute__((hot)) extern int od_query_format(char *format_pos,
						char *format_end,
						kiwi_var_t *user, char *peer,
						char *output, int output_len);

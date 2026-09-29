#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi/kiwi.h>
#include <machinarium/machinarium.h>

#include <types.h>
#include <server.h>

/* On success, result is the single DataRow, or NULL if no rows were returned. */
int od_query_do(od_server_t *server, char *context, const char *query,
		char *param, uint32_t timeout_ms, machine_msg_t **result);

__attribute__((hot)) extern int od_query_format(char *format_pos,
						char *format_end,
						kiwi_var_t *user, char *peer,
						char *output, int output_len);

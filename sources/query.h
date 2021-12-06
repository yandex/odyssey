#ifndef ODYSSEY_QUERY_H
#define ODYSSEY_QUERY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

extern int od_query_do(od_server_t *server, char *query,
				   kiwi_var_t *user, kiwi_password_t *result);

__attribute__((hot)) extern int
od_query_format(char *format_pos, char *format_end, kiwi_var_t *user, char *peer,
		     char *output, int output_len);

#endif /* ODYSSEY_QUERY_H */

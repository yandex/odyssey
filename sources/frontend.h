#ifndef ODYSSEY_FRONTEND_H
#define ODYSSEY_FRONTEND_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

static inline machine_msg_t*
od_frontend_error_msg(od_client_t *client, char *code, char *fmt, va_list args)
{
	char msg[512];
	int  msg_len;
	msg_len = od_snprintf(msg, sizeof(msg), "odyssey: %s%.*s: ",
	                      client->id.id_prefix,
	                      (signed)sizeof(client->id.id),
	                      client->id.id);
	msg_len += od_vsnprintf(msg + msg_len, sizeof(msg) - msg_len, fmt, args);
	return kiwi_be_write_error(code, msg, msg_len);
}

static inline machine_msg_t*
od_frontend_errorf(od_client_t *client, char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_error_msg(client, code, fmt, args);
	va_end(args);
	return msg;
}

int  od_frontend_error(od_client_t*, char*, char*, ...);
void od_frontend(void*);

#endif /* ODYSSEY_FRONTEND_H */

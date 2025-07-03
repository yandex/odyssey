#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#define MAX_STARTUP_ATTEMPTS 7

static inline machine_msg_t *od_frontend_error_msg(od_client_t *client,
						   machine_msg_t *stream,
						   char *code, char *fmt,
						   va_list args)
{
	char msg[OD_QRY_MAX_SZ];
	int msg_len;
	msg_len = od_snprintf(msg, sizeof(msg),
			      "odyssey: %s%.*s: ", client->id.id_prefix,
			      (signed)sizeof(client->id.id), client->id.id);
	msg_len +=
		od_vsnprintf(msg + msg_len, sizeof(msg) - msg_len, fmt, args);
	return kiwi_be_write_error(stream, code, msg, msg_len);
}

static inline machine_msg_t *od_frontend_fatal_msg(od_client_t *client,
						   machine_msg_t *stream,
						   char *code, char *fmt,
						   va_list args)
{
	char msg[OD_QRY_MAX_SZ];
	int msg_len;
	msg_len = od_snprintf(msg, sizeof(msg),
			      "odyssey: %s%.*s: ", client->id.id_prefix,
			      (signed)sizeof(client->id.id), client->id.id);
	msg_len +=
		od_vsnprintf(msg + msg_len, sizeof(msg) - msg_len, fmt, args);
	return kiwi_be_write_error_fatal(stream, code, msg, msg_len);
}

static inline machine_msg_t *od_frontend_errorf(od_client_t *client,
						machine_msg_t *stream,
						char *code, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_error_msg(client, stream, code, fmt, args);
	va_end(args);
	return msg;
}

static inline machine_msg_t *od_frontend_info_msg(od_client_t *client,
						  machine_msg_t *stream,
						  char *fmt, va_list args)
{
	char msg[OD_QRY_MAX_SZ];
	int msg_len;
	msg_len = od_snprintf(msg, sizeof(msg),
			      "odyssey: %s%.*s: ", client->id.id_prefix,
			      (signed)sizeof(client->id.id), client->id.id);
	msg_len +=
		od_vsnprintf(msg + msg_len, sizeof(msg) - msg_len, fmt, args);
	return kiwi_be_write_notice_info(stream, msg, msg_len);
}

static inline machine_msg_t *
od_frontend_infof(od_client_t *client, machine_msg_t *stream, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	machine_msg_t *msg;
	msg = od_frontend_info_msg(client, stream, fmt, args);
	va_end(args);
	return msg;
}

int od_frontend_info(od_client_t *, char *, ...);
int od_frontend_error(od_client_t *, char *, char *, ...);
int od_frontend_fatal(od_client_t *, char *, char *, ...);
void od_frontend(void *);

typedef struct {
	od_storage_endpoint_t *endpoint;
	int priority;
} od_endpoint_attach_candidate_t;

void od_frontend_attach_init_candidates(
	od_instance_t *instance, od_rule_storage_t *storage,
	od_endpoint_attach_candidate_t *candidates,
	od_target_session_attrs_t tsa, int prefer_localhost);

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <machinarium/machinarium.h>

#include <instance.h>
#include <client.h>
#include <util.h>
#include <external_auth.h>

#define AGENT_AUTH_OK 1
#define MAX_MSG_BODY_SIZE (1024 * 1024)
#define TIMEOUT_MS 1000
#define DEFAULT_SOCKET_FILE "/tmp/external-auth.sock"

static void write_header(void *dst, uint64_t header)
{
	uint64_t le = htole64(header);
	memcpy(dst, &le, sizeof(header));
}

static void read_header(uint64_t *header, const void *src)
{
	uint64_t le;
	memcpy(&le, src, sizeof(uint64_t));
	*header = le64toh(le);
}

machine_msg_t *msg_read(mm_io_t *io)
{
	uint64_t body_size = 0;
	machine_msg_t *header = machine_read(io, sizeof(body_size), TIMEOUT_MS);
	if (header == NULL) {
		return NULL;
	}

	read_header(&body_size, machine_msg_data(header));

	machine_msg_free(header);

	if (body_size > MAX_MSG_BODY_SIZE) {
		return NULL;
	}

	return machine_read(io, body_size, TIMEOUT_MS);
}

od_external_auth_status_t msg_write(mm_io_t *io, machine_msg_t *msg)
{
	uint8_t *body = machine_msg_data(msg);
	uint64_t body_size = machine_msg_size(msg);
	machine_msg_t *w = machine_msg_create(sizeof(body_size) + body_size);
	if (w == NULL) {
		machine_msg_free(msg);
		return OD_EAUTH_ERROR;
	}

	uint8_t *pos = machine_msg_data(w);

	write_header(pos, body_size);
	memcpy(pos + sizeof(body_size), body, body_size);

	machine_msg_free(msg);

	int rc = machine_write(io, w, TIMEOUT_MS);
	if (rc < 0) {
		return OD_EAUTH_ERROR;
	}

	return OD_EAUTH_OK;
}

static machine_msg_t *build_cstr_msg(const char *str)
{
	int len = strlen(str) + 1;

	machine_msg_t *msg = machine_msg_create(len);
	if (msg == NULL) {
		return NULL;
	}

	uint8_t *pos = machine_msg_data(msg);
	memcpy(pos, str, len);

	return msg;
}

static od_external_auth_status_t auth_impl(mm_io_t *io, const char *username,
					   const char *token,
					   od_instance_t *instance,
					   od_client_t *client)
{
	machine_msg_t *umsg = build_cstr_msg(username);
	if (umsg == NULL) {
		return OD_EAUTH_ERROR;
	}

	od_external_auth_status_t status = msg_write(io, umsg);
	if (status != OD_EAUTH_OK) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to send user msg to agent, errno=%d (%s)",
			 machine_errno(), strerror(machine_errno()));

		return status;
	}

	machine_msg_t *tmsg = build_cstr_msg(token);
	if (tmsg == NULL) {
		return OD_EAUTH_ERROR;
	}

	status = msg_write(io, tmsg);
	if (status != OD_EAUTH_OK) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to send token msg to agent, errno=%d (%s)",
			 machine_errno(), strerror(machine_errno()));

		return status;
	}

	machine_msg_t *resp = msg_read(io);
	if (resp == NULL) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to receive response from agent, errno=%d (%s)",
			 machine_errno(), strerror(machine_errno()));

		return OD_EAUTH_ERROR;
	}

	if (machine_msg_size(resp) < 1) {
		od_error(&instance->logger, "auth", client, NULL,
			 "invalid msg from agent");
		machine_msg_free(resp);

		return OD_EAUTH_ERROR;
	}

	int success = ((char *)machine_msg_data(resp))[0] == AGENT_AUTH_OK;

	machine_msg_t *eid = msg_read(io);
	if (eid == NULL) {
		od_error(
			&instance->logger, "auth", client, NULL,
			"failed to receive external id from agent, errno=%d (%s)",
			machine_errno(), strerror(machine_errno()));
		machine_msg_free(resp);

		return OD_EAUTH_ERROR;
	}

	char *eid_str = machine_msg_data(eid);
	size_t eid_len = machine_msg_size(eid);

	client->external_id = od_malloc(eid_len + 1);
	if (client->external_id == NULL) {
		machine_msg_free(resp);
		machine_msg_free(eid);

		return OD_EAUTH_ERROR;
	}

	memcpy(client->external_id, eid_str, eid_len);
	client->external_id[eid_len] = '\0';

	if (instance->config.log_session) {
		od_log(&instance->logger, "auth", client, NULL,
		       "user '%s.%s' was authenticated with external_id: %s",
		       client->startup.database.value,
		       client->startup.user.value, client->external_id);
	}

	machine_msg_free(resp);
	machine_msg_free(eid);

	return success ? OD_EAUTH_OK : OD_EAUTH_DENIED;
}

od_external_auth_status_t external_user_authentication(char *username,
						       char *token,
						       od_instance_t *instance,
						       od_client_t *client)
{
	struct sockaddr *saddr;
	struct sockaddr_un exchange_socket;
	memset(&exchange_socket, 0, sizeof(exchange_socket));
	exchange_socket.sun_family = AF_UNIX;
	saddr = (struct sockaddr *)&exchange_socket;

	if (instance->config.external_auth_socket_path == NULL) {
		od_snprintf(exchange_socket.sun_path,
			    sizeof(exchange_socket.sun_path), "%s",
			    DEFAULT_SOCKET_FILE);
	} else {
		od_snprintf(exchange_socket.sun_path,
			    sizeof(exchange_socket.sun_path), "%s",
			    instance->config.external_auth_socket_path);
	}

	od_external_auth_status_t status = OD_EAUTH_DENIED;

	mm_io_t *io = mm_io_create();
	if (io == NULL) {
		return OD_EAUTH_ERROR;
	}

	int rc = mm_io_connect(io, saddr, TIMEOUT_MS);
	if (rc == 0) {
		status = auth_impl(io, username, token, instance, client);
	} else {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to connect to %s, errno=%d (%s)",
			 exchange_socket.sun_path, machine_errno(),
			 strerror(machine_errno()));
		status = OD_EAUTH_ERROR;
	}

	mm_io_close(io);
	mm_io_free(io);

	return status;
}

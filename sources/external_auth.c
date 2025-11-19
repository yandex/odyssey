/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

/*CONNECTION CALLBACK TYPES*/
#define EXTERNAL_AUTH_CONN_ERROR -1
#define EXTERNAL_AUTH_CONN_TIMEOUT -1
#define EXTERNAL_AUTH_CONN_ACCEPTED 0
#define EXTERNAL_AUTH_CONN_DENIED -1

#define EXTERNAL_AUTH_RES_ERROR -1
#define EXTERNAL_AUTH_RES_OK 0

/*AUTHENTICATION TIMEOUT LIMIT*/
#define EXTERNAL_AUTH_DEFAULT_HEADER_SIZE 8
#define EXTERNAL_AUTH_DEFAULT_CNT_CONNECTIONS 1
#define EXTERNAL_AUTH_MAX_MSG_BODY_SIZE 1048576 /* 1 Mb */

#define EXTERNAL_AUTH_DEFAULT_CONNECTION_TIMEOUT 1000
#define EXTERNAL_AUTH_DEFAULT_RECEIVING_HEADER_TIMEOUT 4000
#define EXTERNAL_AUTH_DEFAULT_RECEIVING_BODY_TIMEOUT 1000
#define EXTERNAL_AUTH_DEFAULT_SENDING_TIMEOUT 1000

/*SOCKET FILE*/
#define EXTERNAL_AUTH_DEFAULT_SOCKET_FILE \
	"/tmp/external-auth.sock" /* do NOT recommend to use this socket file path, usually it is not safety */

static void put_header(char dst[], uint64_t src)
{
	for (int i = 0; i < EXTERNAL_AUTH_DEFAULT_HEADER_SIZE; ++i) {
		dst[i] = (src & 0xFF);
		src >>= CHAR_BIT;
	}
}

static void fetch_header(uint64_t *dst, char src[])
{
	*dst = 0; /* Ensure initialization to prevent garbage bits */
	for (int i = 0; i < EXTERNAL_AUTH_DEFAULT_HEADER_SIZE; ++i) {
		(*dst) |= (((uint64_t)(unsigned char)src[i]) << (i * CHAR_BIT));
	}
}

machine_msg_t *external_auth_io_read(machine_io_t *io)
{
	machine_msg_t *header;
	machine_msg_t *msg;

	uint64_t body_size = 0;

	/* RECEIVE HEADER */
	header = machine_read(io, EXTERNAL_AUTH_DEFAULT_HEADER_SIZE,
			      EXTERNAL_AUTH_DEFAULT_RECEIVING_HEADER_TIMEOUT);
	if (header == NULL) {
		return NULL;
	}
	fetch_header(&body_size, (char *)machine_msg_data(header));
	machine_msg_free(header);

	if (body_size > EXTERNAL_AUTH_MAX_MSG_BODY_SIZE) {
		return NULL;
	}
	msg = machine_read(io, body_size,
			   EXTERNAL_AUTH_DEFAULT_RECEIVING_BODY_TIMEOUT);
	if (msg == NULL) {
		return NULL;
	}

	return msg;
}

int external_auth_io_write(machine_io_t *io, machine_msg_t *msg)
{
	/*GET COMMON MSG INFO AND ALLOCATE BUFFER*/
	int32_t send_result = EXTERNAL_AUTH_RES_OK;
	uint64_t body_size = machine_msg_size(
		msg); /* stores size of message (add one byte for 'c\0') */

	/* PREPARE HEADER BUFFER */
	machine_msg_t *header =
		machine_msg_create(EXTERNAL_AUTH_DEFAULT_HEADER_SIZE);
	if (header == NULL) {
		send_result = EXTERNAL_AUTH_RES_ERROR;
		goto free_msg;
	}
	put_header((char *)machine_msg_data(header), body_size);

	/*SEND HEADER TO SOCKET*/
	if (machine_write(io, header, EXTERNAL_AUTH_DEFAULT_SENDING_TIMEOUT) <
	    0) {
		send_result = EXTERNAL_AUTH_RES_ERROR;
		goto free_msg;
	}

	/*SEND MSG TO SOCKET*/
	if (machine_write(io, msg, EXTERNAL_AUTH_DEFAULT_SENDING_TIMEOUT) < 0) {
		send_result = EXTERNAL_AUTH_RES_ERROR;
		goto free_end;
	}
	goto free_end;

free_msg:
	machine_msg_free(
		msg); /* guarantee that the memory will be freed when the function is completed */
free_end:
	return send_result;
}

int external_user_authentication(
	char *username,
	char *token, /* remove const because machine_msg_write use as buf - non constant values (but do nothing ith them....) */
	od_instance_t *instance, od_client_t *client)
{
	int32_t authentication_result =
		EXTERNAL_AUTH_CONN_DENIED; /* stores authenticate status for user (default value: CONN_DENIED) */
	int32_t correct_sending =
		EXTERNAL_AUTH_CONN_ACCEPTED; /* stores status of sending data to external agent */
	char *auth_status_char;
	machine_msg_t *msg_username = NULL, *msg_token = NULL,
		      *auth_status = NULL, *external_user = NULL;

	/*SOCKET SETUP*/
	struct sockaddr *saddr;
	struct sockaddr_un
		exchange_socket; /* socket for interprocceses connection */
	memset(&exchange_socket, 0, sizeof(exchange_socket));
	exchange_socket.sun_family = AF_UNIX;
	saddr = (struct sockaddr *)&exchange_socket;
	/* if socket path set use config value, if it's NULL use default */
	if (instance->config.external_auth_socket_path == NULL) {
		od_snprintf(exchange_socket.sun_path,
			    sizeof(exchange_socket.sun_path), "%s",
			    EXTERNAL_AUTH_DEFAULT_SOCKET_FILE);
	} else {
		od_snprintf(exchange_socket.sun_path,
			    sizeof(exchange_socket.sun_path), "%s",
			    instance->config.external_auth_socket_path);
	}

	/*SETUP IO*/
	machine_io_t *io;
	io = machine_io_create();
	if (io == NULL) {
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_end;
	}

	/*CONNECT TO SOCKET*/
	int rc = machine_connect(io, saddr,
				 EXTERNAL_AUTH_DEFAULT_CONNECTION_TIMEOUT);
	if (rc == NOT_OK_RESPONSE) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to connect to %s", exchange_socket.sun_path);
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_io;
	}

	/*COMMUNICATE WITH SOCKET*/
	msg_username = machine_msg_create(0);
	if (msg_username == NULL) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to allocate msg_username");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_io;
	}
	if (machine_msg_write(msg_username, username, strlen(username) + 1) <
	    0) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to send username to msg_username");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_io;
	}

	msg_token = machine_msg_create(0);
	if (msg_token == NULL) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to allocate msg_token");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_io;
	}
	if (machine_msg_write(msg_token, token, strlen(token) + 1) < 0) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to write token to msg_token");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_io;
	}

	correct_sending = external_auth_io_write(
		io, msg_username); /* send USERNAME to socket */
	if (correct_sending !=
	    EXTERNAL_AUTH_RES_OK) { /* error during sending data to socket */
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to send username to external agent");
		authentication_result = correct_sending;
		/* have guarantee that msg_username have been freed, but need to free msg_token */
		machine_msg_free(msg_token);
		goto free_io;
	}
	correct_sending = external_auth_io_write(
		io, msg_token); /* send TOKEN to socket */
	if (correct_sending !=
	    EXTERNAL_AUTH_RES_OK) { /* error during sending data to socket */
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to send token to external agent");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		/* have guarantee that msg_token have been already freed */
		goto free_io;
	}

	/*COMMUNUCATE WITH SOCKET*/
	auth_status =
		external_auth_io_read(io); /* receive auth_status from socket */
	if (auth_status == NULL ||
	    machine_msg_size(auth_status) <
		    1) { /* receiving is not completed successfully */
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to receive auth_status from external agent");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_io;
	}

	auth_status_char = (char *)machine_msg_data(auth_status);
	if ((unsigned)auth_status_char[0]) {
		authentication_result = EXTERNAL_AUTH_CONN_ACCEPTED;
	} else {
		authentication_result = EXTERNAL_AUTH_CONN_DENIED;
	}

	external_user =
		external_auth_io_read(io); /* receive subject_id from socket */
	if (external_user == NULL) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to receive external_user from external agent");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_auth_status;
	}

	client->external_id = od_calloc(machine_msg_size(external_user) + 1,
					sizeof(*client->external_id));
	if (client->external_id == NULL) {
		od_error(&instance->logger, "auth", client, NULL,
			 "failed to allocate external_id");
		authentication_result = EXTERNAL_AUTH_CONN_ERROR;
		goto free_external_user;
	}
	memcpy(client->external_id, (char *)machine_msg_data(external_user),
	       machine_msg_size(external_user));

	od_log(&instance->logger, "auth", client, NULL,
	       "user '%s.%s', with client_id: %s%.*s was authenticated with external_id: %s",
	       client->startup.database.value, client->startup.user.value,
	       client->id.id_prefix, OD_ID_LEN, client->id.id,
	       client->external_id);

	/*FREE RESOURCES*/
free_external_user:
	if (external_user != NULL) {
		machine_msg_free(external_user);
	}
free_auth_status:
	if (auth_status != NULL) {
		machine_msg_free(auth_status);
	}
free_io:
	machine_close(io);
	machine_io_free(io);
free_end:
	/*RETURN RESULT*/
	return authentication_result;
}

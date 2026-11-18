
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <machinarium/machinarium.h>

#include <instance.h>
#include <client.h>
#include <dns.h>
#include <util.h>
#include <wait_event.h>

/*CONNECTION CALLBACK TYPES*/
#define MDB_IAMPROXY_CONN_ERROR -1
#define MDB_IAMPROXY_CONN_TIMEOUT -1
#define MDB_IAMPROXY_CONN_ACCEPTED 0
#define MDB_IAMPROXY_CONN_DENIED -1

/*AUTHENTICATION TIMEOUT LIMIT*/
#define MDB_IAMPROXY_DEFAULT_HEADER_SIZE 8
#define MDB_IAMPROXY_DEFAULT_CNT_CONNECTIONS 1
#define MDB_IAMPROXY_MAX_MSG_BODY_SIZE 1048576 /* 1 Mb */
#define MDB_IAMPROXY_PROTOCOL_VERSION 2

#define MDB_IAMPROXY_DEFAULT_CONNECTION_TIMEOUT 1000
#define MDB_IAMPROXY_DEFAULT_RECEIVING_HEADER_TIMEOUT 4000
#define MDB_IAMPROXY_DEFAULT_RECEIVING_BODY_TIMEOUT 1000
#define MDB_IAMPROXY_DEFAULT_SENDING_TIMEOUT 1000

/*PAM SOCKET FILE*/
#define MDB_IAMPROXY_DEFAULT_SOCKET_FILE \
	"/var/run/iam-auth-proxy/iam-auth-proxy.sock" /* PAM SOCKET FILE place */

static void put_header(char dst[], uint64_t src)
{
	for (int i = 0; i < MDB_IAMPROXY_DEFAULT_HEADER_SIZE; ++i) {
		dst[i] = (src & 0xFF);
		src >>= CHAR_BIT;
	}
}

static void fetch_header(uint64_t *dst, char src[])
{
	for (int i = 0; i < MDB_IAMPROXY_DEFAULT_HEADER_SIZE; ++i) {
		(*dst) |= (((uint64_t)src[i]) << (i * CHAR_BIT));
	}
}

machine_msg_t *mdb_iamproxy_io_read(mm_io_t *io)
{
	machine_msg_t *header;
	machine_msg_t *msg;

	uint64_t body_size = 0;

	/* RECEIVE HEADER */
	od_wait_event_start(OD_WAIT_AUTH_EXTERNAL);
	header = machine_read(io, MDB_IAMPROXY_DEFAULT_HEADER_SIZE,
			      MDB_IAMPROXY_DEFAULT_RECEIVING_HEADER_TIMEOUT);
	if (header == NULL) {
		od_wait_event_end(OD_WAIT_AUTH_EXTERNAL);
		return NULL;
	}
	fetch_header(&body_size, (char *)machine_msg_data(header));
	machine_msg_free(header);

	if (body_size > MDB_IAMPROXY_MAX_MSG_BODY_SIZE) {
		od_wait_event_end(OD_WAIT_AUTH_EXTERNAL);
		return NULL;
	}
	msg = machine_read(io, body_size,
			   MDB_IAMPROXY_DEFAULT_RECEIVING_BODY_TIMEOUT);
	od_wait_event_end(OD_WAIT_AUTH_EXTERNAL);
	if (msg == NULL) {
		return NULL;
	}

	return msg;
}

static int mdb_iamproxy_io_write(mm_io_t *io, const char *value,
				 uint16_t version)
{
	size_t body_size = strlen(value) + 1;
	if (body_size > MDB_IAMPROXY_MAX_MSG_BODY_SIZE) {
		return MDB_IAMPROXY_CONN_ERROR;
	}

	machine_msg_t *msg = machine_msg_create(body_size);
	if (msg == NULL) {
		return MDB_IAMPROXY_CONN_ERROR;
	}
	memcpy(machine_msg_data(msg), value, body_size);

	machine_msg_t *header =
		machine_msg_create(MDB_IAMPROXY_DEFAULT_HEADER_SIZE);
	if (header == NULL) {
		machine_msg_free(msg);
		return MDB_IAMPROXY_CONN_ERROR;
	}
	/* Only the username header uses the upper 16 bits for the version. */
	put_header(machine_msg_data(header),
		   body_size | ((uint64_t)version << 48));

	if (machine_write(io, header, MDB_IAMPROXY_DEFAULT_SENDING_TIMEOUT) <
	    0) {
		machine_msg_free(msg);
		return MDB_IAMPROXY_CONN_ERROR;
	}

	return machine_write(io, msg, MDB_IAMPROXY_DEFAULT_SENDING_TIMEOUT);
}

int mdb_iamproxy_authenticate_user(char *username, char *token,
				   od_instance_t *instance, od_client_t *client)
{
	int32_t authentication_result =
		MDB_IAMPROXY_CONN_DENIED; /* stores authenticate status for user (default value: CONN_DENIED) */
	char *auth_status_char;
	machine_msg_t *auth_status = NULL, *external_user = NULL;

	/*SOCKET SETUP*/
	struct sockaddr *saddr;
	struct sockaddr_un
		exchange_socket; /* socket for interprocceses connection */
	memset(&exchange_socket, 0, sizeof(exchange_socket));
	exchange_socket.sun_family = AF_UNIX;
	saddr = (struct sockaddr *)&exchange_socket;
	/* if socket path set use config value, if it's NULL use default */
	if (client->rule->mdb_iamproxy_socket_path == NULL) {
		od_snprintf(exchange_socket.sun_path,
			    sizeof(exchange_socket.sun_path), "%s",
			    MDB_IAMPROXY_DEFAULT_SOCKET_FILE);
	} else {
		od_snprintf(exchange_socket.sun_path,
			    sizeof(exchange_socket.sun_path), "%s",
			    client->rule->mdb_iamproxy_socket_path);
	}

	/*SETUP IO*/
	mm_io_t *io;
	io = mm_io_create();
	if (io == NULL) {
		authentication_result = MDB_IAMPROXY_CONN_ERROR;
		goto free_end;
	}

	/*CONNECT TO SOCKET*/
	int rc = mm_io_connect(io, saddr,
			       MDB_IAMPROXY_DEFAULT_CONNECTION_TIMEOUT);
	if (rc == NOT_OK_RESPONSE) {
		od_error(&instance->logger, client, NULL,
			 "failed to connect to %s", exchange_socket.sun_path);
		authentication_result = MDB_IAMPROXY_CONN_ERROR;
		goto free_end;
	}

	if (mdb_iamproxy_io_write(io, username, MDB_IAMPROXY_PROTOCOL_VERSION) <
	    0) {
		od_error(&instance->logger, client, NULL,
			 "failed to send username to iam-auth-proxy");
		authentication_result = MDB_IAMPROXY_CONN_ERROR;
		goto free_io;
	}
	if (mdb_iamproxy_io_write(io, token, 0) < 0) {
		od_error(&instance->logger, client, NULL,
			 "failed to send token to iam-auth-proxy");
		authentication_result = MDB_IAMPROXY_CONN_ERROR;
		goto free_io;
	}

	char client_ip[INET6_ADDRSTRLEN] = "";
	struct sockaddr_storage peer;
	int peer_len = sizeof(peer);
	if (machine_getpeername(client->io.io, (struct sockaddr *)&peer,
				&peer_len) == 0 &&
	    (peer.ss_family == AF_INET || peer.ss_family == AF_INET6)) {
		od_getsockaddrname((struct sockaddr *)&peer, client_ip,
				   sizeof(client_ip), 1, 0);
	}
	if (mdb_iamproxy_io_write(io, client_ip, 0) < 0) {
		od_error(&instance->logger, client, NULL,
			 "failed to send client IP to iam-auth-proxy");
		authentication_result = MDB_IAMPROXY_CONN_ERROR;
		goto free_io;
	}

	/*COMMUNUCATE WITH SOCKET*/
	auth_status =
		mdb_iamproxy_io_read(io); /* receive auth_status from socket */
	if (auth_status == NULL ||
	    machine_msg_size(auth_status) <
		    1) { /* receiving is not completed successfully */
		od_error(&instance->logger, client, NULL,
			 "failed to receive auth_status from iam-auth-proxy");
		authentication_result = MDB_IAMPROXY_CONN_ERROR;
		goto free_io;
	}

	auth_status_char = (char *)machine_msg_data(auth_status);
	if ((unsigned)auth_status_char[0]) {
		authentication_result = MDB_IAMPROXY_CONN_ACCEPTED;
	} else {
		authentication_result = MDB_IAMPROXY_CONN_DENIED;
	}

	external_user =
		mdb_iamproxy_io_read(io); /* receive subject_id from socket */
	if (external_user == NULL) {
		od_error(&instance->logger, client, NULL,
			 "failed to receive external_user from iam-auth-proxy");
		authentication_result = MDB_IAMPROXY_CONN_ERROR;
		goto free_auth_status;
	}

	client->external_id = od_malloc(machine_msg_size(external_user));
	memcpy(client->external_id, (char *)machine_msg_data(external_user),
	       machine_msg_size(external_user));

	od_log(&instance->logger, client, NULL,
	       "user '%s.%s', with client_id: %s%.*s was authenticated by iam with subject_id: %s",
	       client->startup.database.value, client->startup.user.value,
	       client->id.id_prefix, OD_ID_LEN, client->id.id,
	       client->external_id);

	/*FREE RESOURCES*/
	machine_msg_free(external_user);
free_auth_status:
	machine_msg_free(auth_status);
free_io:
	mm_io_close(io);
	mm_io_free(io);
free_end:
	/*RETURN RESULT*/
	return authentication_result;
}

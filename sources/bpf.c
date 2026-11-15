
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <bpf.h>

#ifdef HAVE_EBPF

#include <machinarium/io.h>

#include <global.h>
#include <instance.h>
#include <logger.h>
#include <io.h>
#include <client.h>
#include <server.h>

#include <bpf/libbpf.h>
#include <bpf/bpf.h>

extern unsigned char od_bpf_pipe_object[];
extern unsigned long long od_bpf_pipe_object_size;

typedef struct od_bpf {
	struct bpf_object *object;
	int enabled;
	int client_sockets_map;
	int server_sockets_map;
	int client_states_map;
	int server_states_map;
} od_bpf_t;

static od_bpf_t od_bpf_state;

int od_bpf_init(od_global_t *global)
{
	od_instance_t *instance = global->instance;

	memset(&od_bpf_state, 0, sizeof(od_bpf_state));

	libbpf_set_print(NULL);

	struct bpf_object *object = bpf_object__open_mem(
		od_bpf_pipe_object, od_bpf_pipe_object_size, NULL);
	if (object == NULL) {
		int err_ = errno;
		od_error(
			&instance->logger, NULL, NULL,
			"eBPF: can't open embedded pipe object, userspace pipe will be used, errno=%d (%s)",
			err_, strerror(err_));
		return -1;
	}

	if (bpf_object__load(object) < 0) {
		int err_ = errno;
		od_error(
			&instance->logger, NULL, NULL,
			"eBPF: kernel refused the pipe program, userspace pipe will be used, errno=%d (%s)",
			err_, strerror(err_));
		bpf_object__close(object);
		return -1;
	}

#define find_map(dst, name)                                                  \
	do {                                                                 \
		struct bpf_map *ttt =                                        \
			bpf_object__find_map_by_name(object, name);          \
		if (name == NULL) {                                          \
			int err_ = errno;                                    \
			od_error(&instance->logger, NULL, NULL,              \
				 "eBPF: can't find map '%s', errno=%d (%s)", \
				 name, err_, strerror(err_));                \
			bpf_object__close(object);                           \
			return -1;                                           \
		}                                                            \
		(dst) = bpf_map__fd(ttt);                                    \
	} while (0)

	find_map(od_bpf_state.client_sockets_map, "client_sockets");
	find_map(od_bpf_state.server_sockets_map, "server_sockets");
	find_map(od_bpf_state.client_states_map, "client_states");
	find_map(od_bpf_state.server_states_map, "server_states");

#undef find_map

#define attach_program(map, name)                                                 \
	do {                                                                      \
		struct bpf_program *prog =                                        \
			bpf_object__find_program_by_name(object, name);           \
		if (name == NULL) {                                               \
			int err_ = errno;                                         \
			od_error(                                                 \
				&instance->logger, NULL, NULL,                    \
				"eBPF: can't find program '%s', errno=%d (%s)",   \
				name, err_, strerror(err_));                      \
			bpf_object__close(object);                                \
			return -1;                                                \
		}                                                                 \
		if (bpf_prog_attach(bpf_program__fd(prog), (map),                 \
				    BPF_SK_SKB_VERDICT, 0) < 0) {                 \
			int err_ = errno;                                         \
			od_error(                                                 \
				&instance->logger, NULL, NULL,                    \
				"eBPF: can't attach program '%s', errno=%d (%s)", \
				name, err_, strerror(err_));                      \
			bpf_object__close(object);                                \
			return -1;                                                \
		}                                                                 \
	} while (0)

	attach_program(od_bpf_state.client_sockets_map, "client_fwd");
	attach_program(od_bpf_state.server_sockets_map, "server_fwd");

#undef attach_program

	od_bpf_state.object = object;
	od_bpf_state.enabled = 1;

	od_log(&instance->logger, NULL, NULL, "eBPF: kernel-side is ready");

	return 0;
}

void od_bpf_shutdown(void)
{
	if (od_bpf_state.object != NULL) {
		bpf_object__close(od_bpf_state.object);
	}
	memset(&od_bpf_state, 0, sizeof(od_bpf_state));
}

struct pipe_state {
	__u32 sink;
};

static int port_of(int fd, int peer, __u32 *port)
{
	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);

	int rc;
	if (peer) {
		rc = getpeername(fd, (struct sockaddr *)&ss, &len);
	} else {
		rc = getsockname(fd, (struct sockaddr *)&ss, &len);
	}

	if (rc < 0) {
		return -1;
	}

	if (ss.ss_family == AF_INET) {
		*port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
		return 0;
	} else if (ss.ss_family == AF_INET6) {
		*port = ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
		return 0;
	}

	return -1;
}

static int is_plain(mm_io_t *io)
{
	return io->bio == &io->bio_slots[MM_BIO_SOCKET];
}

static int is_quiescent(od_io_t *io)
{
	if (od_readahead_unread(&io->readahead) != 0) {
		return 0;
	}

	char c;
	ssize_t rc = recv(io->io->fd, &c, 1, MSG_PEEK | MSG_DONTWAIT);
	return rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
}

int od_bpf_pipe_enter(od_client_t *client, od_server_t *server)
{
	if (!od_bpf_state.enabled) {
		return -1;
	}

	od_instance_t *instance = client->global->instance;

	if (!is_plain(client->io.io) || !is_plain(server->io.io)) {
		od_log(&instance->logger, client, server,
		       "eBPF: tls/compression layer present");
		return -1;
	}

	if (client->io.io->is_unix_socket) {
		return -1;
	}

	if (!is_quiescent(&client->io)) {
		od_log(&instance->logger, client, server,
		       "eBPF: client already has pending data");
		return -1;
	}

	if (!is_quiescent(&server->io)) {
		od_log(&instance->logger, client, server,
		       "eBPF: server already has pending data");
		return -1;
	}

	__u32 ckey, skey;
	if (port_of(client->io.io->fd, 1, &ckey) < 0 ||
	    port_of(server->io.io->fd, 0, &skey) < 0) {
		return -1;
	}

	struct pipe_state cst = { .sink = skey };
	struct pipe_state sst = { .sink = ckey };

	int rc = bpf_map_update_elem(od_bpf_state.client_states_map, &ckey,
				     &cst, BPF_ANY);
	if (rc < 0) {
		od_error(&instance->logger, client, server,
			 "eBPF: can't set pipe on client side: %s",
			 strerror(errno));
		return -1;
	}

	rc = bpf_map_update_elem(od_bpf_state.server_states_map, &skey, &sst,
				 BPF_ANY);
	if (rc < 0) {
		od_error(&instance->logger, client, server,
			 "eBPF: can't set pipe on server side: %s",
			 strerror(errno));
		bpf_map_delete_elem(od_bpf_state.client_states_map, &ckey);
		return -1;
	}

	rc = bpf_map_update_elem(od_bpf_state.client_sockets_map, &ckey,
				 &client->io.io->fd, BPF_NOEXIST);
	if (rc < 0) {
		od_error(&instance->logger, client, server,
			 "eBPF: can't set client socket map: %s",
			 strerror(errno));
		bpf_map_delete_elem(od_bpf_state.server_states_map, &skey);
		bpf_map_delete_elem(od_bpf_state.client_states_map, &ckey);
		return -1;
	}

	rc = bpf_map_update_elem(od_bpf_state.server_sockets_map, &skey,
				 &server->io.io->fd, BPF_NOEXIST);
	if (rc < 0) {
		od_error(&instance->logger, client, server,
			 "eBPF: can't set server socket map: %s",
			 strerror(errno));
		bpf_map_delete_elem(od_bpf_state.server_states_map, &skey);
		bpf_map_delete_elem(od_bpf_state.client_states_map, &ckey);
		bpf_map_delete_elem(od_bpf_state.client_sockets_map, &ckey);
		return -1;
	}

	od_log(&instance->logger, client, server,
	       "replication pipe moved to the kernel");

	return 0;
}

void od_bpf_pipe_exit(od_client_t *client, od_server_t *server)
{
	if (!od_bpf_state.enabled) {
		return;
	}

	__u32 ckey, skey;
	int cl = port_of(client->io.io->fd, 1, &ckey) == 0;
	int srv = port_of(server->io.io->fd, 0, &skey) == 0;

	if (cl) {
		bpf_map_delete_elem(od_bpf_state.client_sockets_map, &ckey);
		bpf_map_delete_elem(od_bpf_state.client_states_map, &ckey);
	}

	if (srv) {
		bpf_map_delete_elem(od_bpf_state.server_sockets_map, &skey);
		bpf_map_delete_elem(od_bpf_state.server_states_map, &skey);
	}
}

int od_bpf_pipe_available(void)
{
	return od_bpf_state.enabled;
}

#else /* HAVE_EBPF */

int od_bpf_init(od_global_t *global)
{
	(void)global;
	return -1;
}

void od_bpf_shutdown(void)
{
}

int od_bpf_pipe_enter(od_client_t *client, od_server_t *server)
{
	(void)client;
	(void)server;
	return -1;
}

void od_bpf_pipe_exit(od_client_t *client, od_server_t *server)
{
	(void)client;
	(void)server;
}

int od_bpf_pipe_available(void)
{
	return 0;
}

#endif /* HAVE_EBPF */

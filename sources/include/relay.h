#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <sys/uio.h>

#include <machinarium/machinarium.h>

#include <list.h>
#include <status.h>
#include <client.h>

typedef struct {
	machine_msg_t *msg;
} od_xbuf_msg_t;

typedef struct {
	od_xbuf_msg_t *msgs;
	struct iovec *vecs;
	size_t count;
	size_t capacity;
} od_relay_xbuf_t;

struct od_relay {
	od_client_t *client;
	od_relay_xbuf_t xbuf;
};

void od_relay_init(od_relay_t *relay, od_client_t *client);
void od_relay_destroy(od_relay_t *relay);

od_frontend_status_t od_relay_process_query(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms);
od_frontend_status_t od_relay_process_fcall(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms);
od_frontend_status_t od_relay_process_xmsg(od_relay_t *relay,
					   machine_msg_t *msg,
					   uint32_t timeout_ms);
od_frontend_status_t od_relay_process_xflush(od_relay_t *relay,
					     machine_msg_t *msg,
					     uint32_t timeout_ms);
od_frontend_status_t od_relay_process_xsync(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms);

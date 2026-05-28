#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <sys/uio.h>

#include <machinarium/machinarium.h>
#include <machinarium/ds/vector.h>

#include <list.h>
#include <status.h>
#include <client.h>
#include <xplan.h>

typedef struct {
	machine_msg_t *msg;
} od_xbuf_msg_t;

struct od_relay_xbuf {
	mm_vector_t msgs;
};

struct od_relay {
	od_client_t *client;
	od_relay_xbuf_t xbuf;
	od_xplan_t xplan;

	/* additional message to handle in copy stream */
	machine_msg_t *copy_additional;
};

void od_relay_init(od_relay_t *relay, od_client_t *client);
void od_relay_destroy(od_relay_t *relay);

static inline void od_relay_set_copy_additional(od_relay_t *relay,
						machine_msg_t *msg)
{
	relay->copy_additional = msg;
}

static inline machine_msg_t *od_relay_get_copy_additional(od_relay_t *relay)
{
	return relay->copy_additional;
}

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

extern uint8_t od_relay_deffered_begin_bytes[12];

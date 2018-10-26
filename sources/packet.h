#ifndef ODYSSEY_PACKET_H
#define ODYSSEY_PACKET_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_packet od_packet_t;

struct od_packet
{
	int      has_header;
	uint32_t size;
	uint32_t read;
	uint32_t read_chunk;
};

static inline void
od_packet_init(od_packet_t *packet)
{
	packet->read = 0;
	packet->size = 0;
	packet->read_chunk = 0;
	packet->has_header = 0;
}

static inline void
od_packet_set_chunk(od_packet_t *packet, uint32_t chunk)
{
	packet->read_chunk = chunk;
}

static inline void
od_packet_reset(od_packet_t *packet)
{
	packet->read = 0;
	packet->size = 0;
	packet->has_header = 0;
}

static inline int
od_packet_is_complete(od_packet_t *packet)
{
	return packet->size == 0;
}

static inline int
od_packet_read(od_packet_t *packet, machine_io_t *io, machine_msg_t **msg)
{
	/* read header and first chunk in one message or
	   continue reading chunks one by one */
	int next_chunk = packet->has_header;

	if (! next_chunk)
	{
		*msg = machine_read(io, sizeof(kiwi_header_t), UINT32_MAX);
		if (*msg == NULL)
			return -1;
		char *data = machine_msg_get_data(*msg);
		packet->size = kiwi_read_size(data, machine_msg_get_size(*msg));
		packet->has_header = 1;
	}

	uint32_t to_read = packet->size - packet->read;
	if (to_read > packet->read_chunk)
		to_read = packet->read_chunk;

	int rc;
	if (! next_chunk) {
		rc = machine_read_to(io, *msg, to_read, UINT32_MAX);
		if (rc == -1) {
			machine_msg_free(*msg);
			*msg = NULL;
			return -1;
		}
	} else {
		*msg = machine_read(io, to_read, UINT32_MAX);
		if (*msg == NULL)
			return -1;
	}

	packet->read += to_read;
	if (packet->read == packet->size)
		od_packet_reset(packet);

	return next_chunk;
}

#endif /* ODYSSEY_PACKET_H */

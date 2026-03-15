/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>

#include <client.h>
#include <server.h>
#include <io.h>
#include <list.h>
#include <backend.h>
#include <instance.h>
#include <route.h>
#include <global.h>
#include <stat.h>
#include <misc.h>
#include <stream.h>

static kiwi_be_type_t full_msg_types[] = { KIWI_BE_READY_FOR_QUERY,
					   KIWI_BE_ERROR_RESPONSE,
					   KIWI_BE_PARAMETER_STATUS };

static int is_fully_handled_msg(kiwi_be_type_t type)
{
	for (size_t i = 0; i < sizeof(full_msg_types) / sizeof(kiwi_be_type_t);
	     ++i) {
		if (full_msg_types[i] == type) {
			return 1;
		}
	}

	return 0;
}

typedef struct {
	/*
	 * accumulated message to handle
	 * and amount of bytes of it for read completion
	 */
	machine_msg_t *msg;
	size_t msg_wpos;
	size_t msg_left;

	/* amount of bytes that must not be handled - just streamed */
	size_t skip_left;
} stream_t;

static void stream_init(stream_t *stream)
{
	stream->msg = NULL;
	stream->msg_wpos = 0;
	stream->msg_left = 0;
	stream->skip_left = 0;
}

static void stream_msg_free(stream_t *stream)
{
	if (stream->msg != NULL) {
		machine_msg_free(stream->msg);
	}
	stream->msg = NULL;
	stream->msg_left = 0;
	stream->msg_wpos = 0;
}

static int stream_message_in_progress(stream_t *stream)
{
	if (stream->msg != NULL) {
		return 1;
	}

	if (stream->skip_left > 0) {
		return 1;
	}

	return 0;
}

static od_frontend_status_t stream_handle_message(char *ctx, od_server_t *server,
						  char *data, size_t size,
						  int is_service, int *rfq)
{
	if (od_unlikely(size < sizeof(kiwi_header_t))) {
		/*
		 * this function can be called at non full message
		 *
		 * but can not be called on message with not enough
		 * bytes to read header
		 */
		abort();
	}

	int rc;
	od_client_t *client = server->client;
	od_route_t *route = server->route;
	od_instance_t *instance = server->global->instance;

	kiwi_header_t *hdr = (kiwi_header_t *)data;
	kiwi_be_type_t type = hdr->type;
	uint32_t body = kiwi_read_size(data, sizeof(kiwi_header_t));
	body -= sizeof(uint32_t);
	size_t packet_size = sizeof(kiwi_header_t) + body;

	if (is_fully_handled_msg(type)) {
		if (od_unlikely(packet_size != size)) {
			abort();
		}
	}

	if (instance->config.log_debug) {
		od_debug(&instance->logger, ctx, client, server, "%s",
			 kiwi_be_type_to_string(type));
	}

	switch (type) {
	case KIWI_BE_ERROR_RESPONSE:
		/* we expect stream will accumulate this type of message fully */
		od_backend_error(server, ctx, data, size);
		if (is_service) {
			return OD_ESERVER_READ;
		}
		break;

	case KIWI_BE_PARAMETER_STATUS:
		/* we expect stream will accumulate this type of message fully */
		rc = od_backend_update_parameter(server, ctx, data, size, 0);
		if (rc == -1) {
			od_gerror(ctx, client, server, "can't update parameter");
			return OD_ESERVER_READ;
		}
		break;

	case KIWI_BE_COPY_IN_RESPONSE:
		/* we expect stream will accumulate this type of message */
		server->in_out_response_received++;
		if (is_service) {
			break;
		}

		abort();
		break;
	case KIWI_BE_COPY_OUT_RESPONSE:
		server->in_out_response_received++;
		break;
	case KIWI_BE_COPY_DONE:
		/*
		 * should go after copy out
		 * states that backend copy ended
		 */
		server->done_fail_response_received++;
		break;

	case KIWI_BE_READY_FOR_QUERY:
		/* we expect stream will accumulate this type of message */
		*rfq = 1;

		od_backend_ready(server, data, size);

		if (is_service) {
			break;
		}

		int64_t query_time = 0;
		od_stat_query_end(&route->stats, &server->stats_state,
				  server->is_transaction, &query_time);
		if (instance->config.log_debug && query_time > 0) {
			od_log(&instance->logger, ctx, server->client,
			       server, "query time: %" PRIi64 " microseconds",
			       query_time);
		}
		break;
	default:
		/* nothing to do */
		break;
	}

	return OD_OK;
}

static od_frontend_status_t stream_eat(char *ctx, stream_t *stream, od_server_t *server,
				       int is_service, char *data, size_t size,
				       size_t *processed, int *rfq)
{
	*processed = 0;

	if (stream->skip_left > 0) {
		/* some bytes must be streamed - just account it and exit */
		size_t nskip = od_min(stream->skip_left, size);
		stream->skip_left -= nskip;
		*processed = nskip;
		return OD_OK;
	}

	if (stream->msg_left > 0) {
		/* some bytes must be added to accumulated msg - lets do it */
		size_t nmsg = od_min(stream->msg_left, size);
		stream->msg_left -= nmsg;
		*processed = nmsg;

		char *dest = machine_msg_data(stream->msg);
		memcpy(dest + stream->msg_wpos, data, nmsg);
		stream->msg_wpos += nmsg;

		if (stream->msg_left == 0) {
			/* message was accumulated fully - must handle it */
			od_frontend_status_t st = stream_handle_message(
				ctx, server, dest, machine_msg_size(stream->msg),
				is_service, rfq);
			if (st != OD_OK) {
				return st;
			}
			stream_msg_free(stream);
		}

		return OD_OK;
	}

	/* new package */

	if (size < sizeof(kiwi_header_t)) {
		/* package cannot be recognized - read more */
		return OD_UNDEF;
	}

	kiwi_header_t *hdr = (kiwi_header_t *)data;
	kiwi_be_type_t type = hdr->type;

	uint32_t body;
	int rc = kiwi_validate_header(data, sizeof(kiwi_header_t), &body);
	if (rc != 0) {
		od_gerror(ctx, server->client, server,
			  "invalid package header from server");
		return OD_ESERVER_READ;
	}
	body -= sizeof(uint32_t);
	size_t packet_size = sizeof(kiwi_header_t) + body;

	if (size >= packet_size) {
		*processed = packet_size;

		/* there are enough bytes to process packet */
		od_frontend_status_t st = stream_handle_message(
			ctx, server, data, packet_size, is_service, rfq);
		if (st != OD_OK) {
			return st;
		}
		return OD_OK;
	}

	*processed = size;

	if (is_fully_handled_msg(type)) {
		/* start the accumulating */
		stream->msg = machine_msg_create(packet_size);
		if (stream->msg == NULL) {
			return OD_EOOM;
		}

		char *dest = machine_msg_data(stream->msg);
		memcpy(dest, data, size);
		stream->msg_wpos = size;
		stream->msg_left = packet_size - size;

		return OD_OK;
	}

	/* no need to accumulate but need to skip some bytes in future */
	stream->skip_left = packet_size - size;

	return OD_OK;
}

static void stream_destroy(stream_t *stream)
{
	stream_msg_free(stream);
}

static od_frontend_status_t process_readahead_session(char *ctx, stream_t *stream,
						      od_server_t *server,
						      int is_service, int *rfq,
						      uint32_t timeout_ms)
{
	od_client_t *client = server->client;
	od_readahead_t *readahead = &server->io.readahead;
	struct iovec rvec = od_readahead_read_begin(readahead);
	size_t left = rvec.iov_len;
	char *pos = rvec.iov_base;
	od_frontend_status_t status = OD_OK;
	size_t total_processed = 0;

	while (!(*rfq) && left > 0) {
		size_t processed = 0;
		status = stream_eat(
			ctx, stream, server, is_service, pos, left, &processed, rfq);
		total_processed += processed;
		pos += processed;
		left -= processed;
		if (status == OD_UNDEF) {
			/*
			 * not enough bytes - wait for more
			 *
			 * since it is the situation, where we can't read
			 * packet header - its ok to just repeat read:
			 * there must be enough place in readahead to read the
			 * header - otherwise there is some unread packet that was
			 * processed in previous iteration
			 */
			status = OD_OK;
			break;
		}

		if (status != OD_OK) {
			break;
		}

		if (!stream_message_in_progress(stream)) {
			/*
			 * maybe client disconnected during query execution?
			 * it is better to find it out on message end instead of
			 * the moment at message half-read
			 */
			if (!is_service && client != NULL && !od_io_connected(&client->io)) {
				status = OD_ECLIENT_READ;
				goto to_return;
			}
		}
	}

	if (total_processed > 0 && !is_service) {
		size_t unused;
		int rc = od_io_write_raw(&server->client->io, rvec.iov_base,
					 total_processed, &unused, timeout_ms);
		if (rc != 0) {
			status = OD_ECLIENT_WRITE;
		}
	}

to_return:
	od_readahead_read_commit(readahead, total_processed);
	return status;
}

od_frontend_status_t
od_session_stream_server_until_rfq_impl(char *ctx, od_server_t *server, int is_service,
					uint32_t timeout_ms)
{
	od_frontend_status_t status;
	int rc;

	int rfq = 0;

	od_client_t *client = server->client;

	stream_t stream;
	stream_init(&stream);

	while (!rfq) {
		status = process_readahead_session(ctx, &stream, server, is_service,
						   &rfq, timeout_ms);
		if (status != OD_OK) {
			break;
		}

		if (rfq) {
			break;
		}

		/*
		 * maybe client disconnected during query execution?
		 */
		if (!is_service && client != NULL && !od_io_connected(&client->io)) {
			status = OD_ECLIENT_READ;
			break;
		}

		rc = od_io_read_some(&server->io, timeout_ms);
		if (rc == -1) {
			status = OD_ESERVER_READ;
			break;
		}
	}

	if (status == OD_ECLIENT_READ || status == OD_ECLIENT_WRITE) {
		/*
		 * TODO: wait for message end here instead of just
		 * close server connection on reset
		 */
		if (stream_message_in_progress(&stream)) {
			server->msg_broken = 1;
		}
	}

	stream_destroy(&stream);

	return status;
}

od_frontend_status_t od_session_stream_server_until_rfq(char *ctx, od_server_t *server,
							uint32_t timeout_ms)
{
	return od_session_stream_server_until_rfq_impl(
		ctx, server, 0 /* is_service */, timeout_ms);
}

od_frontend_status_t od_service_stream_server_until_rfq(char *ctx, od_server_t *server,
						       uint32_t timeout_ms)
{
	return od_session_stream_server_until_rfq_impl(
		ctx, server, 1 /* is_service */, timeout_ms);
}
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>

#include <status.h>
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

/*
 * https://www.postgresql.org/docs/current/protocol-flow.html
 */

static int is_response(kiwi_be_type_t type)
{
	/* note: Describe leads to ParametrDescription* + RowDescription | NoData */

	switch (type) {
	case KIWI_BE_PARSE_COMPLETE:
	case KIWI_BE_BIND_COMPLETE:
	case KIWI_BE_CLOSE_COMPLETE:
	case KIWI_BE_COMMAND_COMPLETE:
	case KIWI_BE_EMPTY_QUERY_RESPONSE:
	case KIWI_BE_PORTAL_SUSPENDED:
	case KIWI_BE_ERROR_RESPONSE:
	case KIWI_BE_NO_DATA:
	case KIWI_BE_ROW_DESCRIPTION:
	case KIWI_BE_READY_FOR_QUERY:
		return 1;
	default:
		return 0;
	}
}

enum { STOP_BY_RFQ, STOP_BY_NRESPONSES };

typedef struct {
	int type;
	int rfq;
	int nresponse;
} stream_stop_t;

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

	/* stop criteria */
	stream_stop_t stop;

	od_stream_on_responce_cb on_response;
	void *on_reponse_arg;
} stream_t;

static int server_is_fully_handled(stream_t *stream, kiwi_be_type_t type)
{
	/* this are parsed so only full-mode handling */
	switch (type) {
	case KIWI_BE_READY_FOR_QUERY:
	case KIWI_BE_ERROR_RESPONSE:
	case KIWI_BE_PARAMETER_STATUS:
	case KIWI_BE_COPY_IN_RESPONSE:
	case KIWI_BE_COPY_OUT_RESPONSE:
	case KIWI_BE_COMMAND_COMPLETE:
		return 1;
	default:
		break;
	}

	/*
	 * in STOP_BY_NRESPONSES mode we need to accumulate response
	 * terminators fully to correctly handle them and decrement counter,
	 * otherwise a partial packet could be processed as a terminator
	 */
	if (stream->stop.type == STOP_BY_NRESPONSES) {
		return is_response(type);
	}

	return 0;
}

static inline int stream_must_stop(stream_t *stream)
{
	stream_stop_t *stop = &stream->stop;

	switch (stop->type) {
	case STOP_BY_RFQ:
		return stop->rfq;
	case STOP_BY_NRESPONSES:
		return stop->rfq || stop->nresponse == 0;
	default:
		abort();
	}
}

static void stream_init(stream_t *stream, int nresponses,
			od_stream_on_responce_cb on_response, void *arg)
{
	stream->msg = NULL;
	stream->msg_wpos = 0;
	stream->msg_left = 0;
	stream->skip_left = 0;
	stream->on_response = on_response;
	stream->on_reponse_arg = arg;

	memset(&stream->stop, 0, sizeof(stream_stop_t));
	if (nresponses > 0) {
		stream->stop.type = STOP_BY_NRESPONSES;
		stream->stop.nresponse = nresponses;
	} else {
		stream->stop.type = STOP_BY_RFQ;
	}
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

static inline void handle_server_command_complete(od_client_t *client,
						  od_server_t *server,
						  const char *data)
{
	/* tag is null-terminated */
	const char *command_tag = data + sizeof(kiwi_header_t);

	if (strcmp(command_tag, "LISTEN") == 0) {
		od_glog("main", client, server,
			"got CommandCompelete message with tag LISTEN, client is pinned to server");
		server->client_pinned = 1;
	}
}

static od_frontend_status_t stream_handle_message(stream_t *stream, char *ctx,
						  od_server_t *server,
						  char *data, size_t size,
						  int is_service,
						  int ignore_errors)
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

	if (server_is_fully_handled(stream, type)) {
		if (od_unlikely(packet_size != size)) {
			abort();
		}
	}

	if (instance->config.log_debug) {
		if (type == KIWI_BE_COMMAND_COMPLETE) {
			const char *command_tag = data + sizeof(kiwi_header_t);
			od_debug(&instance->logger, ctx, client, server,
				 "%s - %s", kiwi_be_type_to_string(type),
				 command_tag);
		} else {
			od_debug(&instance->logger, ctx, client, server, "%s",
				 kiwi_be_type_to_string(type));
		}
	}

	switch (type) {
	case KIWI_BE_ERROR_RESPONSE:
		/* we expect stream will accumulate this type of message fully */
		od_backend_error(server, ctx, data, size);

		if (server->xproto_mode) {
			/*
			 * when awaiting for exact count of responses
			 * (on flush) meeting error means that all other
			 * messages will be ignored until Sync
			 */
			if (stream->stop.type == STOP_BY_NRESPONSES) {
				/* will be decreased below to 0 */
				stream->stop.nresponse = 1;
			}
			server->xproto_err = 1;
		}

		if (is_service && !ignore_errors) {
			return OD_ESERVER_READ;
		}

		break;

	case KIWI_BE_PARAMETER_STATUS:
		/* we expect stream will accumulate this type of message fully */
		rc = od_backend_update_parameter(server, ctx, data, size,
						 is_service);
		if (rc == -1) {
			od_gerror(ctx, client, server,
				  "can't update parameter");
			return OD_ESERVER_READ;
		}
		break;

	case KIWI_BE_COPY_IN_RESPONSE:
		server->copy_mode = 1;
		return OD_COPY_IN_RECEIVED;
	case KIWI_BE_COPY_OUT_RESPONSE:
		server->copy_mode = 1;

		if (is_service) {
			/* cant handle that in service streaming */
			return OD_ESERVER_READ;
		}

		/*
		 * DataRow* here until RFQ - ok
		 */
		break;
	case KIWI_BE_COPY_DONE:
		assert(server->copy_mode);
		server->copy_mode = 0;
		break;
	case KIWI_BE_COPY_BOTH_RESPONSE:
		/* replication must never be done by this function */
		abort();

	case KIWI_BE_READY_FOR_QUERY:
		/* we expect stream will accumulate this type of message */
		stream->stop.rfq = 1;

		/* rfq means copy is done (written for case copy errored) */
		server->copy_mode = 0;

		/*
		 * reset xproto state - now can process
		 * all new xproto messages
		 */
		server->xproto_mode = 0;
		server->xproto_err = 0;

		od_backend_ready(server, data, size);

		if (is_service) {
			break;
		}

		int64_t query_time = 0;
		od_stat_query_end(&route->stats, &server->stats_state,
				  server->is_transaction, &query_time);
		if (instance->config.log_debug && query_time > 0) {
			od_log(&instance->logger, ctx, server->client, server,
			       "query time: %" PRIi64 " microseconds",
			       query_time);
		}
		break;
	case KIWI_BE_COMMAND_COMPLETE:
		if (is_service) {
			break;
		}

		if (!route->rule->pool->pin_on_listen) {
			break;
		}

		handle_server_command_complete(client, server, data);
		break;
	default:
		/* nothing to do */
		break;
	}

	if (is_response(type)) {
		if (stream->stop.type == STOP_BY_NRESPONSES) {
			assert(stream->stop.nresponse > 0);
			--stream->stop.nresponse;
		}

		if (stream->on_response != NULL) {
			od_frontend_status_t st = stream->on_response(
				type, stream->on_reponse_arg);

			if (st != OD_OK) {
				return st;
			}
		}
	}

	return OD_OK;
}

static od_frontend_status_t stream_eat(char *ctx, stream_t *stream,
				       od_server_t *server, int is_service,
				       int ignore_errors, char *data,
				       size_t size, size_t *processed)
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
				stream, ctx, server, dest,
				machine_msg_size(stream->msg), is_service,
				ignore_errors);
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
		od_frontend_status_t st =
			stream_handle_message(stream, ctx, server, data,
					      packet_size, is_service,
					      ignore_errors);
		if (st != OD_OK) {
			return st;
		}
		return OD_OK;
	}

	*processed = size;

	if (server_is_fully_handled(stream, type)) {
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

static od_frontend_status_t process_readahead(char *ctx, stream_t *stream,
					      od_server_t *server,
					      int is_service, int ignore_errors,
					      uint32_t timeout_ms)
{
	od_client_t *client = server->client;
	od_readahead_t *readahead = &server->io.readahead;
	struct iovec rvec = od_readahead_read_begin(readahead);
	size_t left = rvec.iov_len;
	char *pos = rvec.iov_base;
	od_frontend_status_t status = OD_OK;
	size_t total_processed = 0;

	while (!stream_must_stop(stream) && left > 0) {
		size_t processed = 0;
		status = stream_eat(ctx, stream, server, is_service,
				    ignore_errors, pos, left, &processed);
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
			if (!is_service && client != NULL &&
			    !od_io_connected(&client->io)) {
				status = OD_ECLIENT_READ;
				goto to_return;
			}
		}
	}

	if (total_processed > 0 && !is_service) {
		size_t unused;
		int rc = od_io_write_raw(&client->io, rvec.iov_base,
					 total_processed, &unused, timeout_ms);
		if (rc != 0) {
			status = OD_ECLIENT_WRITE;
		}
	}

to_return:
	od_readahead_read_commit(readahead, total_processed);
	return status;
}

static int readahead_empty(od_readahead_t *ra)
{
	return od_readahead_unread(ra) == 0;
}

static od_frontend_status_t
od_stream_server_impl(char *ctx, od_server_t *server, int is_service,
		      int ignore_errors, int nresponses,
		      od_stream_on_responce_cb on_response, void *arg,
		      uint32_t timeout_ms)
{
	od_frontend_status_t status = OD_OK;
	int rc;

	od_client_t *client = server->client;

	stream_t stream;
	stream_init(&stream, nresponses, on_response, arg);

	/*
	 * set up the server io operations to be interrupted if
	 * something happens on client io
	 * this is done for disconnect detection
	 */
	int peering = !is_service && client != NULL;
	if (peering) {
		od_io_set_peer(&server->io, &client->io);
	}

	/* TODO: support more correct timeout */
	while (!stream_must_stop(&stream)) {
		status = process_readahead(ctx, &stream, server, is_service,
					   ignore_errors, timeout_ms);
		if (status != OD_OK && status != OD_UNDEF) {
			break;
		}

		if (stream_must_stop(&stream)) {
			break;
		}

		/*
		 * maybe client disconnected during query execution?
		 */
		if (peering && !od_io_connected(&client->io)) {
			status = OD_ECLIENT_READ;
			break;
		}

		/*
		 * must read only in cases of:
		 * - readahead is fully processed
		 * - readahead contains no enough bytes (OD_UNDEF)
		 */
		if (!readahead_empty(&server->io.readahead) &&
		    status != OD_UNDEF) {
			continue;
		}

		rc = od_io_read_some(&server->io, timeout_ms);
		if (rc == -1) {
			int errno_ = machine_errno();
			if (errno_ == EAGAIN) {
				/* something happened on peer (client->io), retry */
				continue;
			}

			/* timeout or any other error - give up */
			status = OD_ESERVER_READ;
			break;
		}
	}

	if (peering) {
		od_io_remove_peer(&server->io, &client->io);
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

od_frontend_status_t od_stream_server_until_rfq(char *ctx, od_server_t *server,
						uint32_t timeout_ms)
{
	return od_stream_server_impl(ctx, server, 0 /* is_service */,
				     0 /* ignore_errors */, 0 /* nresponses */,
				     NULL, NULL, timeout_ms);
}

od_frontend_status_t od_service_stream_server_until_rfq(char *ctx,
							od_server_t *server,
							int ignore_errors,
							uint32_t timeout_ms)
{
	return od_stream_server_impl(ctx, server, 1 /* is_service */,
				     ignore_errors, 0 /* nresponses */, NULL,
				     NULL, timeout_ms);
}

od_frontend_status_t
od_stream_server_exact_completes(char *ctx, od_server_t *server, int n,
				 od_stream_on_responce_cb on_response,
				 void *arg, uint32_t timeout_ms)
{
	return od_stream_server_impl(ctx, server, 0 /* is_service */,
				     0 /* ignore_errors */, n /* nresponses */,
				     on_response, arg, timeout_ms);
}

od_frontend_status_t od_stream_server_sync(char *ctx, od_server_t *server,
					   uint32_t timeout_ms)
{
	return od_stream_server_impl(ctx, server, 0 /* is_service */,
				     0 /* ignore_errors */, 0 /* nresponses */,
				     NULL, NULL, timeout_ms);
}

/*
 * similar to stream from server,
 * but no need to read packages fully
 *
 * so just proxy the bytes and track the packet headers
 */
typedef struct {
	size_t left;
	int done_fail_met;
} copy_stream_t;

static void copy_stream_init(copy_stream_t *cs)
{
	cs->left = 0;
	cs->done_fail_met = 0;
}

static void copy_stream_destroy(copy_stream_t *cs)
{
	(void)cs;
}

static od_frontend_status_t
copy_handle_msg(copy_stream_t *cs, od_client_t *client, kiwi_fe_type_t type)
{
	(void)client;

	switch (type) {
	case KIWI_FE_COPY_DONE:
	case KIWI_FE_COPY_FAIL:
		/*
		 * we are not done yet, in case the msg is read non-fully
		 * so mark done to be set on next package end
		 */
		cs->done_fail_met = 1;
		break;

	/* Sync and Flush messages are ignored by pg but allowed to be sent */
	case KIWI_FE_SYNC:
	case KIWI_FE_FLUSH:
	case KIWI_FE_COPY_DATA:
		break;

	default:
		return OD_ECLIENT_PROTOCOL_ERROR;
	}

	return OD_OK;
}

static od_frontend_status_t copy_stream_eat(copy_stream_t *cs, char *ctx,
					    od_client_t *client, char *data,
					    size_t size, size_t *processed)
{
	*processed = 0;

	if (cs->left > 0) {
		size_t nskip = od_min(cs->left, size);
		cs->left -= nskip;
		*processed = nskip;
		return OD_OK;
	}

	/* new package */

	if (size < sizeof(kiwi_header_t)) {
		/* package cannot be recognized - read more */
		return OD_UNDEF;
	}

	kiwi_header_t *hdr = (kiwi_header_t *)data;
	kiwi_fe_type_t type = hdr->type;

	uint32_t body;
	int rc = kiwi_validate_header(data, sizeof(kiwi_header_t), &body);
	if (rc != 0) {
		od_gerror(ctx, client, client->server,
			  "invalid package header from client");
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	body -= sizeof(uint32_t);
	size_t packet_size = sizeof(kiwi_header_t) + body;

	od_frontend_status_t st = copy_handle_msg(cs, client, type);
	if (st != OD_OK) {
		return st;
	}

	if (size >= packet_size) {
		*processed = packet_size;

		/*
		 * there are enough bytes to process message
		 * and the message already processed
		 */
		return OD_OK;
	}

	*processed = size;

	cs->left = packet_size - size;

	return OD_OK;
}

static int copy_stream_stop(copy_stream_t *cs)
{
	return cs->left == 0 && cs->done_fail_met;
}

static od_frontend_status_t
copy_process_readahead(char *ctx, copy_stream_t *stream, od_client_t *client,
		       int stop_on_bound, uint32_t timeout_ms)
{
	od_server_t *server = client->server;
	od_readahead_t *readahead = &client->io.readahead;
	struct iovec rvec = od_readahead_read_begin(readahead);
	size_t left = rvec.iov_len;
	char *pos = rvec.iov_base;
	od_frontend_status_t status = OD_OK;
	size_t total_processed = 0;

	while (!copy_stream_stop(stream) && left > 0) {
		size_t processed = 0;
		status = copy_stream_eat(stream, ctx, client, pos, left,
					 &processed);
		assert(processed <= left);
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
			assert(processed == 0);
			break;
		}

		if (status != OD_OK) {
			assert(processed == 0);
			break;
		}

		if (stream->left == 0 && stop_on_bound) {
			/*
			 * something happened on server, and this is message bound - return
			 *
			 * can't just stop anytime - we could sent a part of msg and to not to
			 * break the protocol must finish message senging
			 */
			break;
		}
	}

	assert(total_processed <= rvec.iov_len);

	if (total_processed > 0) {
		size_t unused;
		int rc = od_io_write_raw(&server->io, rvec.iov_base,
					 total_processed, &unused, timeout_ms);
		assert((rc == 0 && (total_processed == unused)) ||
		       ((total_processed != unused) && rc != 0));
		if (rc != 0) {
			status = OD_ESERVER_WRITE;
		}
	}

	od_readahead_read_commit(readahead, total_processed);
	return status;
}

static od_frontend_status_t stream_copy_from_client(char *ctx,
						    od_client_t *client,
						    machine_msg_t *additional,
						    uint32_t timeout_ms)
{
	od_frontend_status_t status = OD_OK;
	int rc;

	od_server_t *server = client->server;

	copy_stream_t stream;
	copy_stream_init(&stream);

	/*
	 * set up the client io operations to be interrupted if
	 * something happens on server io
	 * this is done for disconnection/errors of server detection
	 */
	od_io_set_peer(&client->io, &server->io);

	if (additional) {
		kiwi_header_t *hdr =
			(kiwi_header_t *)machine_msg_data(additional);
		kiwi_fe_type_t type = hdr->type;
		od_frontend_status_t st =
			copy_handle_msg(&stream, client, type);
		if (st != OD_OK) {
			return st;
		}

		rc = od_io_write(&server->io, additional, timeout_ms);
		if (rc != 0) {
			return OD_ESERVER_WRITE;
		}
	}

	int server_event = 0;

	/* TODO: support more correct timeout */
	while (!copy_stream_stop(&stream)) {
		/*
		 * something happened on server, and this is message bound - return
		 *
		 * can't just stop anytime - we could sent a part of msg and to not to
		 * break the protocol must finish message senging
		 */
		if (stream.left == 0 && server_event) {
			break;
		}

		status = copy_process_readahead(ctx, &stream, client,
						server_event, timeout_ms);
		if (status != OD_OK && status != OD_UNDEF) {
			break;
		}

		if (copy_stream_stop(&stream)) {
			break;
		}

		/*
		 * must read only in cases of:
		 * - readahead is fully processed
		 * - readahead contains no enough bytes (OD_UNDEF)
		 */
		if (!readahead_empty(&client->io.readahead) &&
		    status != OD_UNDEF) {
			continue;
		}

		assert(od_readahead_unread(&client->io.readahead) <
		       (int)sizeof(kiwi_header_t));

		rc = od_io_read_some(&client->io, timeout_ms);
		if (rc == -1) {
			int errno_ = machine_errno();
			if (errno_ == EAGAIN) {
				/*
				 * something happened on peer (server->io)
				 * any other events than write is a signal to stop
				 * the streaming
				 *
				 * if it is write event - this is just sndbuf flush
				 */
				int ev = od_io_last_event(&server->io);
				if (ev & MM_R || ev & MM_ERR || ev & MM_CLOSE) {
					server_event = 1;
				}
				continue;
			}

			/* timeout or any other error - give up */
			status = OD_ECLIENT_READ;
			break;
		}
	}

	od_io_remove_peer(&client->io, &server->io);

	copy_stream_destroy(&stream);

	return status;
}

od_frontend_status_t od_stream_copy_to_server(char *ctx, od_client_t *client,
					      od_server_t *server,
					      machine_msg_t *additional,
					      uint32_t timeout_ms)
{
	/*
	 * stream from client to server all CopyData/CopyFail/CopyDone messages
	 * then stream from server to client until RFQ
	 */

	od_frontend_status_t status =
		stream_copy_from_client(ctx, client, additional, timeout_ms);

	if (status != OD_OK) {
		return status;
	}

	if (server->xproto_mode) {
		server->copy_mode = 0;
		return OD_OK;
	}

	/* server->copy_mode=0 will be set on RFQ */
	status = od_stream_server_until_rfq(ctx, server, timeout_ms);

	return status;
}

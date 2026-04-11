#include <odyssey.h>

#include <kiwi/kiwi.h>

#include <pstmt.h>
#include <server.h>
#include <client.h>
#include <global.h>
#include <instance.h>
#include <backend.h>
#include <route.h>
#include <rules.h>
#include <stream.h>
#include <relay.h>
#include <xplan.h>
#include <misc.h>
#include <util.h>

static inline kiwi_be_type_t msg_be_type(machine_msg_t *msg)
{
	kiwi_be_type_t type = *(uint8_t *)machine_msg_data(msg);

	return type;
}

static inline kiwi_fe_type_t msg_fe_type(machine_msg_t *msg)
{
	kiwi_fe_type_t type = *(uint8_t *)machine_msg_data(msg);

	return type;
}

static const char *plan_entry_type_str(od_xplan_entry_type_t type)
{
	switch (type) {
	case OD_XPLAN_FORWARD:
		return "OD_XPLAN_FORWARD";
	case OD_XPLAN_PARSE:
		return "OD_XPLAN_PARSE";
	case OD_XPLAN_PARSE_SHADOW:
		return "OD_XPLAN_PARSE_SHADOW";
	case OD_XPLAN_VIRTUAL_ERROR_RESPONSE:
		return "OD_XPLAN_VIRTUAL_ERROR_RESPONSE";
	case OD_XPLAN_VIRTUAL_CLOSE_COMPLETE:
		return "OD_XPLAN_VIRTUAL_CLOSE_COMPLETE";
	case OD_XPLAN_VIRTUAL_PARSE_COMPLETE:
		return "OD_XPLAN_VIRTUAL_PARSE_COMPLETE";
	default:
		abort();
	}
}

static machine_msg_t *plan_entry_backend_msg(od_xplan_entry_t *entry)
{
	switch (entry->type) {
	case OD_XPLAN_FORWARD:
		if (entry->srvmsg != NULL) {
			return entry->srvmsg;
		} else if (entry->clmsg != NULL) {
			return entry->clmsg;
		} else {
			abort();
		}
		break;
	case OD_XPLAN_PARSE:
	case OD_XPLAN_PARSE_SHADOW:
		if (entry->srvmsg != NULL) {
			return entry->srvmsg;
		}
		abort();
		break;
	case OD_XPLAN_VIRTUAL_ERROR_RESPONSE:
	case OD_XPLAN_VIRTUAL_PARSE_COMPLETE:
	case OD_XPLAN_VIRTUAL_CLOSE_COMPLETE:
		return NULL;
	default:
		abort();
	}
}

static const char *plan_delta_type_str(od_xplan_delta_type_t type)
{
	switch (type) {
	case OD_XPLAN_DELTA_NONE:
		return "OD_XPLAN_DELTA_NONE";
	case OD_XPLAN_DELTA_ADD_BOTH:
		return "OD_XPLAN_DELTA_ADD_BOTH";
	case OD_XPLAN_DELTA_ADD_CLIENT_ONLY:
		return "OD_XPLAN_DELTA_ADD_CLIENT_ONLY";
	case OD_XPLAN_DELTA_ADD_SERVER_ONLY:
		return "OD_XPLAN_DELTA_ADD_SERVER_ONLY";
	case OD_XPLAN_DELTA_REMOVE_CLIENT_ONLY:
		return "OD_XPLAN_DELTA_REMOVE_CLIENT_ONLY";
	case OD_XPLAN_DELTA_REMOVE_ALL:
		return "OD_XPLAN_DELTA_REMOVE_ALL";
	default:
		abort();
	}
}

static int plan_delta_description(od_xplan_delta_t *delta, char *out,
				  size_t max)
{
	char *pos = out;
	char *end = out + max;

	pos += od_snprintf(pos, end - pos,
			   "type=%s, client_pstmt='%s', pstmt=%s",
			   plan_delta_type_str(delta->type),
			   delta->client_pstmt,
			   delta->pstmt ? delta->pstmt->name : "(null)");

	return pos - out;
}

static int plan_entry_description(od_xplan_entry_t *entry, char *out,
				  size_t max)
{
	char *pos = out;
	char *end = out + max;
	pos += od_snprintf(pos, end - pos, "[");

	pos += od_snprintf(pos, end - pos, "%s",
			   plan_entry_type_str(entry->type));

	machine_msg_t *bmsg = plan_entry_backend_msg(entry);
	if (bmsg != NULL) {
		pos += od_snprintf(pos, end - pos, ", bmsg=%c",
				   msg_fe_type(bmsg));
	}

	pos += od_snprintf(pos, end - pos, ", delta=(");
	pos += plan_delta_description(&entry->delta, pos, end - pos);
	pos += od_snprintf(pos, end - pos, ")]");

	return pos - out;
}

static inline machine_msg_t *pstmt_already_exists_msg(const char *name)
{
	char buf[OD_QRY_MAX_SZ];
	int len = od_snprintf(buf, sizeof(buf),
			      "prepared statement \"%s\" already exists", name);

	return kiwi_be_write_error(NULL, KIWI_DUPLICATE_PSTATEMENT, buf, len);
}

static inline machine_msg_t *pstmt_does_not_exists_msg(const char *name)
{
	char buf[OD_QRY_MAX_SZ];
	int len;
	if (name[0] != '\0') {
		len = od_snprintf(buf, sizeof(buf),
				  "prepared statement \"%s\" does not exist",
				  name);
	} else {
		len = od_snprintf(buf, sizeof(buf),
				  "unnamed prepared statement does not exist");
	}

	return kiwi_be_write_error(NULL, KIWI_INVALID_SQL_STATEMENT_NAME, buf,
				   len);
}

static void plan_entry_destroy(void *a)
{
	od_xplan_entry_t *entry = a;

	/* entry->clmsg is owned by xbuf, no free */

	machine_msg_free_safe(entry->srvmsg);
	machine_msg_free_safe(entry->vresp);

	/*
	 * no need to destroy delta:
	 * - client name points to original msg
	 * - pstmt points to global hashmap
	 */

	memset(entry, 0, sizeof(od_xplan_entry_t));
}

static void entry_init_internal(od_xplan_entry_t *e, od_xplan_entry_type_t type,
				machine_msg_t *clmsg, machine_msg_t *srvmsg,
				machine_msg_t *vresp,
				od_xplan_delta_type_t delta_type,
				const char *client_pstmt,
				const od_pstmt_t *pstmt)
{
	memset(e, 0, sizeof(od_xplan_entry_t));

	e->type = type;
	e->clmsg = clmsg;
	e->srvmsg = srvmsg;
	e->vresp = vresp;

	e->delta.type = delta_type;
	e->delta.client_pstmt = client_pstmt;
	e->delta.pstmt = pstmt;
}

static void entry_init_fwd(od_xplan_entry_t *e, machine_msg_t *original,
			   machine_msg_t *rewritten,
			   od_xplan_delta_type_t delta_type,
			   const char *client_pstmt, const od_pstmt_t *pstmt)
{
	entry_init_internal(e, OD_XPLAN_FORWARD, original, rewritten, NULL,
			    delta_type, client_pstmt, pstmt);
}

static void entry_init_fwd_no_delta(od_xplan_entry_t *e,
				    machine_msg_t *original,
				    machine_msg_t *rewritten)
{
	entry_init_fwd(e, original, rewritten, OD_XPLAN_DELTA_NONE, NULL, NULL);
}

static void entry_init_parse(od_xplan_entry_t *e, const char *client_pstmt,
			     const od_pstmt_t *pstmt, machine_msg_t *original,
			     machine_msg_t *rewritten)
{
	entry_init_internal(e, OD_XPLAN_PARSE, original, rewritten, NULL,
			    OD_XPLAN_DELTA_ADD_BOTH, client_pstmt, pstmt);
}

static void entry_init_parse_shadow(od_xplan_entry_t *e,
				    const od_pstmt_t *pstmt,
				    machine_msg_t *original,
				    machine_msg_t *inserted_parse)
{
	entry_init_internal(e, OD_XPLAN_PARSE_SHADOW, original, inserted_parse,
			    NULL, OD_XPLAN_DELTA_ADD_SERVER_ONLY, NULL, pstmt);
}

static void entry_init_virtual_error_response(od_xplan_entry_t *e,
					      machine_msg_t *original,
					      machine_msg_t *msg)
{
	entry_init_internal(e, OD_XPLAN_VIRTUAL_ERROR_RESPONSE, original, NULL,
			    msg, OD_XPLAN_DELTA_NONE, NULL, NULL);
}

static void entry_init_virtual_parse_complete(od_xplan_entry_t *e,
					      const char *client_pstmt,
					      const od_pstmt_t *pstmt,
					      machine_msg_t *original)
{
	entry_init_internal(e, OD_XPLAN_VIRTUAL_PARSE_COMPLETE, original, NULL,
			    NULL /* no virtual msg - const seq of bytes */,
			    OD_XPLAN_DELTA_ADD_CLIENT_ONLY, client_pstmt,
			    pstmt);
}

static void entry_init_virtual_close_complete(od_xplan_entry_t *e,
					      const char *client_pstmt,
					      machine_msg_t *original)
{
	entry_init_internal(e, OD_XPLAN_VIRTUAL_CLOSE_COMPLETE, original, NULL,
			    NULL /* no virtual msg - const seq of bytes */,
			    OD_XPLAN_DELTA_REMOVE_CLIENT_ONLY, client_pstmt,
			    NULL);
}

static int xplan_append(od_xplan_t *xp, const od_xplan_entry_t *e)
{
	return mm_vector_append(&xp->entries, e);
}

static inline od_frontend_status_t
xplan_append_fwd(od_xplan_t *xp, machine_msg_t *original,
		 machine_msg_t *rewritten, od_xplan_delta_type_t delta_type,
		 const char *client_pstmt, const od_pstmt_t *pstmt)
{
	od_xplan_entry_t e;
	entry_init_fwd(&e, original, rewritten, delta_type, client_pstmt,
		       pstmt);

	int rc = xplan_append(xp, &e);
	if (rc != 0) {
		return OD_EOOM;
	}

	return OD_OK;
}

static inline od_frontend_status_t
xplan_append_fwd_no_delta(od_xplan_t *xp, machine_msg_t *original,
			  machine_msg_t *rewritten)
{
	od_xplan_entry_t e;
	entry_init_fwd_no_delta(&e, original, rewritten);

	int rc = xplan_append(xp, &e);
	if (rc != 0) {
		return OD_EOOM;
	}

	return OD_OK;
}

static inline od_frontend_status_t xplan_append_parse(od_xplan_t *xp,
						      const char *client_pstmt,
						      const od_pstmt_t *pstmt,
						      machine_msg_t *original,
						      machine_msg_t *rewritten)
{
	od_xplan_entry_t e;
	entry_init_parse(&e, client_pstmt, pstmt, original, rewritten);

	int rc = xplan_append(xp, &e);
	if (rc != 0) {
		return OD_EOOM;
	}

	return OD_OK;
}

static inline od_frontend_status_t
xplan_append_parse_shadow(od_xplan_t *xp, const od_pstmt_t *pstmt,
			  machine_msg_t *original,
			  machine_msg_t *inserted_parse)
{
	od_xplan_entry_t e;
	entry_init_parse_shadow(&e, pstmt, original, inserted_parse);

	int rc = xplan_append(xp, &e);
	if (rc != 0) {
		return OD_EOOM;
	}

	return OD_OK;
}

static inline od_frontend_status_t
xplan_append_virtual_error_response(od_xplan_t *xp, machine_msg_t *original,
				    machine_msg_t *msg)
{
	od_xplan_entry_t e;
	entry_init_virtual_error_response(&e, original, msg);

	int rc = xplan_append(xp, &e);
	if (rc != 0) {
		return OD_EOOM;
	}

	return OD_OK;
}

static inline od_frontend_status_t
xplan_append_virtual_parse_complete(od_xplan_t *xp, const char *client_pstmt,
				    const od_pstmt_t *pstmt,
				    machine_msg_t *original)
{
	od_xplan_entry_t e;
	entry_init_virtual_parse_complete(&e, client_pstmt, pstmt, original);

	int rc = xplan_append(xp, &e);
	if (rc != 0) {
		return OD_EOOM;
	}

	return OD_OK;
}

static inline od_frontend_status_t
xplan_append_virtual_close_complete(od_xplan_t *xp, const char *client_pstmt,
				    machine_msg_t *original)
{
	od_xplan_entry_t e;
	entry_init_virtual_close_complete(&e, client_pstmt, original);

	int rc = xplan_append(xp, &e);
	if (rc != 0) {
		return OD_EOOM;
	}

	return OD_OK;
}

void od_xplan_init(od_xplan_t *xp)
{
	memset(xp, 0, sizeof(od_xplan_t));

	mm_vector_init(&xp->entries, sizeof(od_xplan_entry_t),
		       plan_entry_destroy);
}

void od_xplan_destroy(od_xplan_t *xp)
{
	od_xplan_clear(xp);

	mm_vector_destroy(&xp->entries);
}

void od_xplan_clear(od_xplan_t *xp)
{
	mm_vector_clear(&xp->entries);
}

static const od_pstmt_t *plan_client_get_pstmt(od_xplan_t *xp,
					       od_client_t *client,
					       const char *pstmt_name)
{
	/*
	 * searching from newest state to oldest,
	 * this means to search from tail to head of the plan deltas
	 * and then on commited pstmts hashmap
	 */

	for (int i = (int)mm_vector_size(&xp->entries) - 1; i >= 0; --i) {
		od_xplan_entry_t *e = mm_vector_get(&xp->entries, (size_t)i);
		od_xplan_delta_t *delta = &e->delta;

		switch (delta->type) {
		case OD_XPLAN_DELTA_ADD_SERVER_ONLY:
		case OD_XPLAN_DELTA_NONE:
			/* non-client pstmts op */
			break;

		case OD_XPLAN_DELTA_ADD_CLIENT_ONLY:
			/* fallthrough */
		case OD_XPLAN_DELTA_ADD_BOTH:
			if (strcmp(pstmt_name, delta->client_pstmt) == 0) {
				/* found adding of the pstmt */
				return delta->pstmt;
			}
			break;

		case OD_XPLAN_DELTA_REMOVE_CLIENT_ONLY:
			if (strcmp(pstmt_name, delta->client_pstmt) == 0) {
				/* found removing of the pstmt */
				return NULL;
			}
			break;

		case OD_XPLAN_DELTA_REMOVE_ALL:
			/* all pstmts was removed, including this */
			return NULL;

		default:
			abort();
		}
	}

	return od_client_get_pstmt(client, pstmt_name);
}

static int plan_client_pstmt_exists(od_xplan_t *xp, od_client_t *client,
				    const char *pstmt_name)
{
	return plan_client_get_pstmt(xp, client, pstmt_name) != NULL;
}

static int plan_server_has_pstmt(od_xplan_t *xp, od_server_t *server,
				 const od_pstmt_t *pstmt)
{
	/*
	 * searching from newest state to oldest,
	 * this means to search from tail to head of the plan deltas
	 * and then on commited pstmts hashmap
	 */

	for (int i = (int)mm_vector_size(&xp->entries) - 1; i >= 0; --i) {
		od_xplan_entry_t *e = mm_vector_get(&xp->entries, (size_t)i);
		od_xplan_delta_t *delta = &e->delta;

		switch (delta->type) {
		case OD_XPLAN_DELTA_REMOVE_CLIENT_ONLY:
		case OD_XPLAN_DELTA_ADD_CLIENT_ONLY:
		case OD_XPLAN_DELTA_NONE:
			/* non-server pstmts op */
			break;

		case OD_XPLAN_DELTA_ADD_SERVER_ONLY:
			/* fallthrough */
		case OD_XPLAN_DELTA_ADD_BOTH:
			/* XXX: check by pointers comparison? */
			if (strcmp(pstmt->name, delta->pstmt->name) == 0) {
				/* found adding of the pstmt */
				return 1;
			}
			break;

		case OD_XPLAN_DELTA_REMOVE_ALL:
			/* all pstmts was removed, including this */
			return 0;

		default:
			abort();
		}
	}

	return od_server_has_pstmt(server, pstmt);
}

static inline od_frontend_status_t plan_predeploy_pstmt(od_xplan_t *xp,
							od_server_t *server,
							machine_msg_t *clmsg,
							const od_pstmt_t *pstmt)
{
	if (plan_server_has_pstmt(xp, server, pstmt)) {
		return OD_OK;
	}

	machine_msg_t *pmsg = od_pstmt_parse_of(pstmt);
	if (pmsg == NULL) {
		return OD_EOOM;
	}

	return xplan_append_parse_shadow(xp, pstmt, clmsg, pmsg);
}

static od_frontend_status_t plan_parse(od_relay_t *relay, od_xplan_t *xp,
				       od_xbuf_msg_t *m)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;
	od_instance_t *instance = client->global->instance;

	machine_msg_t *msg = m->msg;
	char *pstmt_name = od_pstmt_name_from_parse(msg);
	if (pstmt_name == NULL) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}

	od_pstmt_desc_t desc = od_pstmt_desc_from_parse(msg);
	if (desc.data == NULL) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}

	/*
	 * special case for unnamed stmt - it should not be check for duplication
	 * because PG allows to do smth like
	 * Parse(""), Parse(""), Parse("")
	 * without getting any error about pstmt already exists
	 */
	if (pstmt_name[0] != '\0' &&
	    plan_client_pstmt_exists(xp, client, pstmt_name)) {
		machine_msg_t *vresp = pstmt_already_exists_msg(pstmt_name);
		if (vresp == NULL) {
			return OD_EOOM;
		}

		return xplan_append_virtual_error_response(xp, msg, vresp);
	}

	mm_hashmap_t *global_pstmts = od_instance_get_pstmts_map(instance);
	od_pstmt_t *pstmt = od_pstmt_create_or_get(global_pstmts, desc);
	if (pstmt == NULL) {
		return OD_EOOM;
	}

	if (plan_server_has_pstmt(xp, server, pstmt)) {
		return xplan_append_virtual_parse_complete(xp, pstmt_name,
							   pstmt, msg);
	}

	machine_msg_t *pmsg = od_pstmt_parse_of(pstmt);
	if (pmsg == NULL) {
		return OD_EOOM;
	}

	return xplan_append_parse(xp, pstmt_name, pstmt, msg, pmsg);
}

static od_frontend_status_t plan_describe(od_relay_t *relay, od_xplan_t *xp,
					  od_xbuf_msg_t *m)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;

	machine_msg_t *msg = m->msg;
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	uint32_t pstmt_name_len;
	char *pstmt_name;
	int rc;
	kiwi_fe_describe_type_t type;
	rc = kiwi_be_read_describe(data, size, &pstmt_name, &pstmt_name_len,
				   &type);
	if (rc == -1) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	if (type == KIWI_FE_DESCRIBE_PORTAL) {
		/* skip this, we only need to rewrite statement */
		return xplan_append_fwd_no_delta(xp, msg,
						 NULL /* no rewrite */);
	}

	const od_pstmt_t *pstmt = plan_client_get_pstmt(xp, client, pstmt_name);
	if (pstmt == NULL) {
		machine_msg_t *vresp = pstmt_does_not_exists_msg(pstmt_name);
		if (vresp == NULL) {
			return OD_EOOM;
		}

		return xplan_append_virtual_error_response(xp, msg, vresp);
	}

	od_frontend_status_t st = plan_predeploy_pstmt(xp, server, msg, pstmt);
	if (st != OD_OK) {
		return st;
	}

	machine_msg_t *dmsg = od_pstmt_describe_of(pstmt);
	if (dmsg == NULL) {
		return OD_EOOM;
	}

	return xplan_append_fwd_no_delta(xp, msg, dmsg);
}

static od_frontend_status_t plan_close(od_relay_t *relay, od_xplan_t *xp,
				       od_xbuf_msg_t *m)
{
	/* close pstmt only on client side */

	od_client_t *client = relay->client;

	machine_msg_t *msg = m->msg;
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	char *pstmt_name = NULL;
	uint32_t unused;
	kiwi_fe_close_type_t type;
	if (kiwi_be_read_close(data, size, &pstmt_name, &unused, &type) != 0) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}

	if (type != KIWI_FE_CLOSE_PREPARED_STATEMENT) {
		return xplan_append_fwd_no_delta(xp, msg,
						 NULL /* no rewrite */);
	}

	if (plan_client_pstmt_exists(xp, client, pstmt_name)) {
		return xplan_append_virtual_close_complete(xp, pstmt_name, msg);
	}

	machine_msg_t *err = pstmt_does_not_exists_msg(pstmt_name);
	if (err == NULL) {
		return OD_EOOM;
	}

	return xplan_append_virtual_error_response(xp, msg, err);
}

static inline machine_msg_t *rewrite_bind_msg(char *data, int size,
					      int opname_start_offset,
					      int operator_name_len,
					      const char *opname, int opnamelen)
{
	machine_msg_t *msg =
		machine_msg_create(size - operator_name_len + opnamelen);
	if (msg == NULL) {
		return msg;
	}

	char *rewrite_data = machine_msg_data(msg);

	/* packet header */
	memcpy(rewrite_data, data, opname_start_offset);
	/* prefix for opname */
	od_snprintf(rewrite_data + opname_start_offset, opnamelen, opname);
	/* rest of msg */
	memcpy(rewrite_data + opname_start_offset + opnamelen,
	       data + opname_start_offset + operator_name_len,
	       size - opname_start_offset - operator_name_len);
	/* set proper size to package */
	kiwi_header_set_size((kiwi_header_t *)rewrite_data,
			     size - operator_name_len + opnamelen);

	return msg;
}

static od_frontend_status_t plan_bind(od_relay_t *relay, od_xplan_t *xp,
				      od_xbuf_msg_t *m)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;
	od_instance_t *instance = client->global->instance;

	machine_msg_t *msg = m->msg;
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	uint32_t pstmt_name_len;
	char *pstmt_name;
	int rc;
	rc = kiwi_be_read_bind_stmt_name(data, size, &pstmt_name,
					 &pstmt_name_len);
	if (rc == -1) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}

	const od_pstmt_t *pstmt = plan_client_get_pstmt(xp, client, pstmt_name);
	if (pstmt == NULL) {
		machine_msg_t *vresp = pstmt_does_not_exists_msg(pstmt_name);
		if (vresp == NULL) {
			return OD_EOOM;
		}

		return xplan_append_virtual_error_response(xp, msg, vresp);
	}

	int remove_all = 0;

	/* XXX: we must plan discard on Execute, not on Bind */
	const od_pstmt_desc_t *desc = &pstmt->desc;
	if (desc->len >= 7) {
		if (strncmp(desc->data, "DISCARD", 7) == 0) {
			od_debug(&instance->logger, "rewrite bind", client,
				 server, "discard detected, invalidate caches");
			remove_all = 1;
		}
	}

	int pstmt_start_offset = kiwi_be_bind_opname_offset(data, size);
	if (pstmt_start_offset < 0) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}

	od_frontend_status_t st = plan_predeploy_pstmt(xp, server, msg, pstmt);
	if (st != OD_OK) {
		return st;
	}

	machine_msg_t *bmsg =
		rewrite_bind_msg(data, size, pstmt_start_offset, pstmt_name_len,
				 pstmt->name, strlen(pstmt->name) + 1);
	if (bmsg == NULL) {
		return OD_EOOM;
	}

	if (!remove_all) {
		return xplan_append_fwd_no_delta(xp, msg, bmsg);
	}

	return xplan_append_fwd(xp, msg, bmsg, OD_XPLAN_DELTA_REMOVE_ALL,
				pstmt_name, pstmt);
}

static od_frontend_status_t plan_execute(od_relay_t *relay, od_xplan_t *xp,
					 od_xbuf_msg_t *m)
{
	(void)relay;

	return xplan_append_fwd_no_delta(xp, m->msg, NULL /* no rewrite */);
}

static od_frontend_status_t plan_flush(od_relay_t *relay, od_xplan_t *xp,
				       od_xbuf_msg_t *m)
{
	(void)relay;

	return xplan_append_fwd_no_delta(xp, m->msg, NULL /* no rewrite */);
}

static od_frontend_status_t plan_sync(od_relay_t *relay, od_xplan_t *xp,
				      od_xbuf_msg_t *m)
{
	(void)relay;

	return xplan_append_fwd_no_delta(xp, m->msg, NULL /* no rewrite */);
}

static od_frontend_status_t plan_simple(od_xplan_t *xp, od_relay_t *relay)
{
	od_relay_xbuf_t *xbuf = &relay->xbuf;

	/* if no prepared statements must be preserved - just send all as it is from xbuf */
	size_t count = mm_vector_size(&xbuf->msgs);

	for (size_t i = 0; i < count; ++i) {
		od_xbuf_msg_t *m = mm_vector_get(&xbuf->msgs, i);

		od_frontend_status_t status =
			xplan_append_fwd_no_delta(xp, m->msg, NULL);
		if (status != OD_OK) {
			return status;
		}
	}

	return OD_OK;
}

static od_frontend_status_t plan_with_pstmt_support(od_xplan_t *xp,
						    od_relay_t *relay)
{
	od_frontend_status_t status = OD_OK;
	od_relay_xbuf_t *xbuf = &relay->xbuf;
	size_t count = mm_vector_size(&xbuf->msgs);

	if (od_unlikely(count == 0)) {
		abort();
	}

	/* handle Flush/Sync out of cycle */
	--count;

	for (size_t i = 0; i < count; ++i) {
		od_xbuf_msg_t *m = mm_vector_get(&xbuf->msgs, i);
		kiwi_fe_type_t type = msg_fe_type(m->msg);

		switch (type) {
		case KIWI_FE_PARSE:
			status = plan_parse(relay, xp, m);
			break;

		case KIWI_FE_DESCRIBE:
			status = plan_describe(relay, xp, m);
			break;

		case KIWI_FE_CLOSE:
			status = plan_close(relay, xp, m);
			break;

		case KIWI_FE_BIND:
			status = plan_bind(relay, xp, m);
			break;

		case KIWI_FE_EXECUTE:
			status = plan_execute(relay, xp, m);
			break;

		default:
			abort();
		}

		if (status != OD_OK) {
			return status;
		}

		if (mm_vector_size(&xp->entries) == 0) {
			continue;
		}

		od_xplan_entry_t *last = mm_vector_back(&xp->entries);
		if (last->type == OD_XPLAN_VIRTUAL_ERROR_RESPONSE) {
			/*
			 * last planned was error response
			 * now skip all until flush/sync (which is last in xbuf)
			 */
			break;
		}
	}

	od_xbuf_msg_t *last = mm_vector_back(&xbuf->msgs);
	kiwi_fe_type_t type = msg_fe_type(last->msg);
	switch (type) {
	case KIWI_FE_FLUSH:
		status = plan_flush(relay, xp, last);
		break;
	case KIWI_FE_SYNC:
		status = plan_sync(relay, xp, last);
		break;
	default:
		abort();
	}

	return status;
}

od_frontend_status_t od_xplan_make_from_xbuf(od_xplan_t *xp, od_relay_t *relay)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;
	od_relay_xbuf_t *xbuf = &relay->xbuf;

	assert(mm_vector_size(&xbuf->msgs) > 0);

	if (server->xproto_err) {
		/*
		 * if is in xproto err state - nothing should be planned
		 * (after error all messages is skipped until Sync)
		 *
		 * so, plan only one forward for the last msg - Flush/Sync
		 */

		od_xbuf_msg_t *last = mm_vector_back(&xbuf->msgs);
		assert(msg_fe_type(last->msg) == KIWI_FE_FLUSH ||
		       msg_fe_type(last->msg) == KIWI_FE_SYNC);

		return xplan_append_fwd_no_delta(xp, last->msg, NULL);
	}

	od_route_t *route = client->route;
	od_rule_t *rule = route->rule;
	int reserve_prepared = rule->pool->reserve_prepared_statement;

	if (!reserve_prepared) {
		return plan_simple(xp, relay);
	}

	return plan_with_pstmt_support(xp, relay);
}

static od_frontend_status_t
delta_apply(od_xplan_delta_t *delta, od_client_t *client, od_server_t *server)
{
	od_xplan_delta_type_t type = delta->type;

	if (type == OD_XPLAN_DELTA_ADD_BOTH ||
	    type == OD_XPLAN_DELTA_ADD_CLIENT_ONLY) {
		int rc = od_client_add_pstmt(client, delta->client_pstmt,
					     delta->pstmt);
		if (rc == -1) {
			return OD_EOOM;
		}
	}

	if (type == OD_XPLAN_DELTA_ADD_BOTH ||
	    type == OD_XPLAN_DELTA_ADD_SERVER_ONLY) {
		int rc = od_server_add_pstmt(server, delta->pstmt);
		if (rc == -1) {
			return OD_EOOM;
		}
	}

	switch (type) {
	case OD_XPLAN_DELTA_NONE:
		/* nothing to do */
		break;
	case OD_XPLAN_DELTA_REMOVE_CLIENT_ONLY:
		od_client_remove_pstmt(client, delta->client_pstmt);
		break;
	case OD_XPLAN_DELTA_REMOVE_ALL:
		od_client_pstmts_clear(client);
		od_server_pstmts_clear(server);
		break;

	case OD_XPLAN_DELTA_ADD_BOTH:
	case OD_XPLAN_DELTA_ADD_CLIENT_ONLY:
	case OD_XPLAN_DELTA_ADD_SERVER_ONLY:
		/* already handled */
		break;

	default:
		abort();
	}

	return OD_OK;
}

static size_t prepare_send_iovecs(od_xplan_t *xp, struct iovec *iovecs)
{
	size_t nentries = mm_vector_size(&xp->entries);
	size_t written = 0;
	int errored = 0;

	for (size_t i = 0; i < nentries; ++i) {
		od_xplan_entry_t *e = mm_vector_get(&xp->entries, i);

		if (errored && i != nentries - 1) {
			/* when virtual error met, skip all until Flush/Sync */
			continue;
		}

		if (e->type == OD_XPLAN_VIRTUAL_ERROR_RESPONSE) {
			errored = 1;
			continue;
		}

		machine_msg_t *msg = plan_entry_backend_msg(e);
		if (msg != NULL) {
			iovecs[written++] = machine_msg_iovec(msg);
		}
	}

	return written;
}

static od_frontend_status_t run_virtual_error(od_xplan_entry_t *error,
					      od_client_t *client,
					      uint32_t timeout_ms)
{
	int rc = od_io_write(&client->io, error->vresp, timeout_ms);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_OK;
}

static od_frontend_status_t run_virtual_parse_complete(od_xplan_entry_t *pc,
						       od_client_t *client,
						       uint32_t timeout_ms)
{
	(void)pc;

	static const uint8_t bytes[] = { KIWI_BE_PARSE_COMPLETE, 0, 0, 0, 4 };
	assert(sizeof(bytes) == 5);

	size_t unused;
	int rc = od_io_write_raw(&client->io, bytes, sizeof(bytes), &unused,
				 timeout_ms);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_OK;
}

static od_frontend_status_t run_virtual_close_complete(od_xplan_entry_t *cc,
						       od_client_t *client,
						       uint32_t timeout_ms)
{
	(void)cc;

	static const uint8_t bytes[] = { KIWI_BE_CLOSE_COMPLETE, 0, 0, 0, 4 };
	assert(sizeof(bytes) == 5);

	size_t unused;
	int rc = od_io_write_raw(&client->io, bytes, sizeof(bytes), &unused,
				 timeout_ms);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_OK;
}

static od_frontend_status_t run_parse_shadow(od_xplan_entry_t *ps,
					     od_client_t *client,
					     od_server_t *server,
					     uint32_t timeout_ms)
{
	(void)ps;

	od_instance_t *instance = client->global->instance;

	machine_msg_t *msg = od_read(&server->io, timeout_ms);
	if (msg == NULL) {
		return OD_ESERVER_READ;
	}

	od_frontend_status_t status;

	kiwi_be_type_t type = msg_be_type(msg);
	switch (type) {
	case KIWI_BE_PARSE_COMPLETE:
		/* drop response - client should not see PC from shadow parse */
		status = OD_OK;
		break;
	case KIWI_BE_ERROR_RESPONSE:
		/*
		 * i think this ErrorResponse is nearly impossible on shadow parsing
		 * but lets handle it, just in case
		 */
		od_backend_error(server, "main", machine_msg_data(msg),
				 machine_msg_size(msg));
		od_error(&instance->logger, "main", client, server,
			 "run_parse_shadow: ErrorResponse met");
		status = OD_ESERVER_READ;
		break;
	default:
		od_error(
			&instance->logger, "main", client, server,
			"run_parse_shadow: unexpected msg type from server '%c'",
			type);
		status = OD_ESERVER_READ;
		break;
	}

	machine_msg_free(msg);

	return status;
}

static od_frontend_status_t run_parse(od_xplan_entry_t *ps, od_client_t *client,
				      od_server_t *server, uint32_t timeout_ms)
{
	(void)ps;
	(void)client;

	return od_stream_server_exact_completes(
		"main", server, 1 /* one complete for parse */, timeout_ms);
}

static od_frontend_status_t run_forward(od_xplan_entry_t *forward,
					od_client_t *client,
					od_server_t *server,
					uint32_t timeout_ms)
{
	(void)forward;

	od_frontend_status_t status = od_stream_server_exact_completes(
		"main", server, 1 /* one complete for 1 forward */, timeout_ms);

	if (status == OD_COPY_IN_RECEIVED) {
		status = od_stream_copy_to_server("main", client, server,
						  timeout_ms);
		if (status == OD_OK) {
			status = OD_COPY_IN_RECEIVED;
		}
	}

	return status;
}

static od_frontend_status_t run_plan_impl(od_xplan_t *xp, od_relay_t *relay,
					  size_t max_responses,
					  uint32_t timeout_ms)
{
	/* TODO: use this to check correctness */
	(void)max_responses;

	od_client_t *client = relay->client;
	od_instance_t *instance = client->global->instance;
	od_rule_t *rule = client->route->rule;
	od_server_t *server = client->server;
	od_frontend_status_t status = OD_OK;

	/*
	 * almost most of plan entries leads to exactly one 'response':
	 * Parse	-> ErrorResponse | ParseComplete
	 * Bind		-> ErrorResponse | BindComplete
	 * Close	-> ErrorResponse | CloseComplete (responded virtually)
	 * Execute	-> DataRow* + ErrorResponse | CommandComplete | EmptyQueryResponse | PortalSuspended
	 * Describe	-> ParameterDescription* + RowDescription | NoData
	 * Sync		-> ReadyForQuery
	 *
	 * individually:
	 * Flush	-> <no response>
	 * no response awaiting from server
	 */

	od_stat_query_start(&server->stats_state);

	if (!server->xproto_mode) {
		/* this is first run of xproto buf after last sync */
		od_server_sync_request(server, 1);
	}

	/*
	 * we are now running the server in xproto mode
	 * this flag will be set to 0 on ReadyForQuery
	 */
	server->xproto_mode = 1;

	size_t count = mm_vector_size(&xp->entries);
	assert(count > 0);

	/* handle Flush/Sync outside of cycle */
	--count;

	for (size_t i = 0; i < count; ++i) {
		od_xplan_entry_t *entry = mm_vector_get(&xp->entries, i);

		if (server->xproto_err) {
			/* if error is met, skip all until Flush/Sync */
			break;
		}

		if (instance->config.log_debug || rule->log_debug) {
			char buf[256];
			plan_entry_description(entry, buf, sizeof(buf));
			od_debug(&instance->logger, "main", client, server,
				 "running %s entry", buf);
		}

		switch (entry->type) {
		case OD_XPLAN_FORWARD:
			status = run_forward(entry, client, server, timeout_ms);
			break;
		case OD_XPLAN_PARSE:
			status = run_parse(entry, client, server, timeout_ms);
			break;
		case OD_XPLAN_PARSE_SHADOW:
			status = run_parse_shadow(entry, client, server,
						  timeout_ms);
			break;
		case OD_XPLAN_VIRTUAL_ERROR_RESPONSE:
			server->xproto_err = 1;

			status = run_virtual_error(entry, client, timeout_ms);
			break;
		case OD_XPLAN_VIRTUAL_PARSE_COMPLETE:
			status = run_virtual_parse_complete(entry, client,
							    timeout_ms);
			break;
		case OD_XPLAN_VIRTUAL_CLOSE_COMPLETE:
			status = run_virtual_close_complete(entry, client,
							    timeout_ms);
			break;
		default:
			abort();
		}

		if (status == OD_COPY_IN_RECEIVED) {
			/* skip all other entries of the plan and go to Sync */
			break;
		}

		if (server->cached_plan_broken) {
			/* server will be dropped */
			status = OD_ESERVER_READ;
		}

		if (status == OD_OK && !server->xproto_err) {
			status = delta_apply(&entry->delta, client, server);
		}

		if (instance->config.log_debug || rule->log_debug) {
			char buf[256];
			plan_entry_description(entry, buf, sizeof(buf));
			od_debug(&instance->logger, "main", client, server,
				 "%s entry finished with status %s", buf,
				 od_frontend_status_to_str(status));
		}

		if (status != OD_OK) {
			return status;
		}
	}

	if (status == OD_COPY_IN_RECEIVED) {
		/* no need to run last step, Sync was sent by stream copy */
		return OD_OK;
	}

	od_xplan_entry_t *last = mm_vector_back(&xp->entries);
	if (od_unlikely(last->type != OD_XPLAN_FORWARD)) {
		abort();
	}
	kiwi_fe_type_t last_type = msg_fe_type(last->clmsg);
	if (last_type == KIWI_FE_FLUSH) {
		/*
		 * no message from server is awaited on Flush
		 * so the plan executing finished
		 *
		 * the server is leaved in xproto state (possible with error)
		 */
		return OD_OK;
	}

	if (last_type == KIWI_FE_SYNC) {
		return od_stream_server_sync("main", server, timeout_ms);
	}

	abort();
}

static od_frontend_status_t
send_buf_and_run_plan(od_xplan_t *xp, od_relay_t *relay, struct iovec *iovecs,
		      size_t niovecs, uint32_t timeout_ms)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;

	int rc = od_io_writev(&server->io, iovecs, niovecs, timeout_ms);
	if (rc == -1) {
		return OD_ESERVER_WRITE;
	}

	/*
	 * use niovecs because it shows real number of how much responses we are awaiting
	 * (some might have been skipped after virtual error response)
	 */
	return run_plan_impl(xp, relay, niovecs, timeout_ms);
}

static inline void log_plan_debug(od_instance_t *instance, od_xplan_t *xp,
				  od_client_t *client)
{
	char *msg = od_malloc(1024);
	if (msg == NULL) {
		return;
	}
	char *end = msg + 1024;
	char *pos = msg;

	size_t count = mm_vector_size(&xp->entries);
	pos += od_snprintf(pos, end - pos, "(");
	for (size_t i = 0; i < count; ++i) {
		od_xplan_entry_t *entry = mm_vector_get(&xp->entries, i);
		pos += plan_entry_description(entry, pos, end - pos);
	}

	pos += od_snprintf(pos, end - pos, ")");
	od_debug(&instance->logger, "main", client, client->server, "xplan: %s",
		 msg);

	od_free(msg);
}

static inline void log_srv_send_buf(od_instance_t *instance,
				    od_client_t *client,
				    const struct iovec *iovecs, size_t niovecs)
{
	char *msg = od_malloc(1024);
	if (msg == NULL) {
		return;
	}
	char *end = msg + 1024;
	char *pos = msg;

	pos += od_snprintf(pos, end - pos, "(");
	for (size_t i = 0; i < niovecs; ++i) {
		struct iovec vec = iovecs[i];
		pos += od_snprintf(pos, end - pos, "{msg=%c, len=%lu}",
				   *(const char *)vec.iov_base, vec.iov_len);
	}

	pos += od_snprintf(pos, end - pos, ")");
	od_debug(&instance->logger, "main", client, client->server,
		 "server buf: %s", msg);

	od_free(msg);
}

static inline void log_xbuf(od_instance_t *instance, od_client_t *client,
			    od_relay_xbuf_t *xbuf)
{
	char *msg = od_malloc(1024);
	if (msg == NULL) {
		return;
	}
	char *end = msg + 1024;
	char *pos = msg;

	pos += od_snprintf(pos, end - pos, "(");
	od_xbuf_msg_t *m = mm_vector_get(&xbuf->msgs, 0);
	pos += od_snprintf(pos, end - pos, "%c", msg_fe_type(m->msg));

	size_t count = mm_vector_size(&xbuf->msgs);
	for (size_t i = 1; i < count; ++i) {
		m = mm_vector_get(&xbuf->msgs, i);
		pos += od_snprintf(pos, end - pos, ", %c", msg_fe_type(m->msg));
	}

	pos += od_snprintf(pos, end - pos, ")");
	od_debug(&instance->logger, "main", client, client->server, "xbuf: %s",
		 msg);

	od_free(msg);
}

od_frontend_status_t od_xplan_run(od_xplan_t *xp, od_relay_t *relay,
				  uint32_t timeout_ms)
{
	od_client_t *client = relay->client;
	od_rule_t *rule = client->route->rule;
	od_instance_t *instance = client->global->instance;

	size_t count = mm_vector_size(&xp->entries);
	assert(count > 0);

	size_t niovecs = count;
	struct iovec *iovecs = od_malloc(sizeof(struct iovec) * niovecs);
	if (iovecs == NULL) {
		return OD_EOOM;
	}

	size_t written = prepare_send_iovecs(xp, iovecs);

	if (instance->config.log_debug || rule->log_debug) {
		log_xbuf(instance, client, &relay->xbuf);
		log_plan_debug(instance, xp, client);
		log_srv_send_buf(instance, client, iovecs, written);
	}

	od_frontend_status_t status =
		send_buf_and_run_plan(xp, relay, iovecs, written, timeout_ms);

	od_free(iovecs);

	return status;
}

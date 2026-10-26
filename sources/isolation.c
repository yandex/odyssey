/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <ctype.h>
#include <limits.h>

#include <isolation.h>
#include <pstmt.h>
#include <client.h>
#include <server.h>
#include <route.h>
#include <global.h>
#include <instance.h>
#include <worker.h>
#include <util.h>
#include <sql/full/parser.h>

static void probe_parse_error(const char *message, void *arg)
{
	(void)message;
	(void)arg;
}

static int read_pid(const char **cursor, const char *end, uint32_t *pid)
{
	const char *p = *cursor;
	while (p < end && isspace((unsigned char)*p)) {
		p++;
	}
	int negative = p < end && *p == '-';
	if (p < end && (*p == '+' || *p == '-')) {
		p++;
	}
	const char *digits = p;
	uint64_t value = 0;
	while (p < end && *p >= '0' && *p <= '9') {
		value = value * 10 + (*p++ - '0');
		if (value > (uint64_t)INT32_MAX + negative) {
			return 0;
		}
	}
	if (p == digits) {
		return 0;
	}
	while (p < end && isspace((unsigned char)*p)) {
		p++;
	}
	*pid = negative ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
	*cursor = p;
	return 1;
}

void od_isolation_probe_parse(od_pstmt_t *pstmt, od_linear_alloc_t *arena)
{
	const char *query = pstmt->desc.data;
	/* Only this uncommon function needs an additional full parse. */
	if (strcasestr(query, "pg_isolation_test_session_is_blocked") == NULL) {
		return;
	}

	size_t len = strlen(query);
	List *tree =
		od_sql_full_parse(arena, query, len, probe_parse_error, NULL);
	if (list_length(tree) != 1) {
		return;
	}
	RawStmt *raw = linitial_node(RawStmt, tree);
	if (!IsA(raw->stmt, SelectStmt)) {
		return;
	}
	SelectStmt *select = castNode(SelectStmt, raw->stmt);
	if (list_length(select->targetList) != 1 || select->fromClause != NIL ||
	    select->whereClause || select->groupClause != NIL ||
	    select->havingClause || select->windowClause != NIL ||
	    select->distinctClause != NIL || select->intoClause ||
	    select->sortClause != NIL || select->limitOffset ||
	    select->limitCount || select->lockingClause != NIL ||
	    select->withClause || select->larg || select->rarg) {
		return;
	}
	ResTarget *target = linitial_node(ResTarget, select->targetList);
	if (!IsA(target->val, FuncCall)) {
		return;
	}
	FuncCall *call = castNode(FuncCall, target->val);
	if (list_length(call->funcname) != 2 ||
	    strcmp(strVal(linitial(call->funcname)), "pg_catalog") != 0 ||
	    strcmp(strVal(lsecond(call->funcname)),
		   "pg_isolation_test_session_is_blocked") != 0 ||
	    list_length(call->args) != 2 || call->agg_order != NIL ||
	    call->agg_filter || call->over || call->agg_star ||
	    call->agg_distinct || call->agg_within_group ||
	    call->func_variadic) {
		return;
	}
	Node *first = linitial(call->args);
	Node *second = lsecond(call->args);
	if (!IsA(first, ParamRef) || castNode(ParamRef, first)->number != 1 ||
	    !IsA(second, A_Const)) {
		return;
	}
	A_Const *array = castNode(A_Const, second);
	if (array->isnull || !IsA(&array->val, String) || array->location < 0) {
		return;
	}

	size_t offset = array->location;
	if (offset + 3 >= len || query[offset] != '\'' ||
	    query[offset + 1] != '{') {
		return;
	}
	const char *close = strchr(query + offset + 2, '\'');
	if (close == NULL || close[-1] != '}') {
		return;
	}
	const char *p = query + offset + 2;
	const char *end = close - 1;
	if (p < end) {
		for (;;) {
			uint32_t pid;
			if (!read_pid(&p, end, &pid)) {
				return;
			}
			if (p == end) {
				break;
			}
			if (*p++ != ',') {
				return;
			}
		}
	}
	pstmt->isolation_pids_offset = offset;
	pstmt->isolation_pids_len = close - (query + offset) + 1;
}

/* The route lock protects both client lifetimes and backend assignments. */
static void snapshot_pids(od_client_t *client, const uint32_t *virtual_pids,
			  uint32_t *backend_pids, size_t count)
{
	od_route_t *route = client->route;
	od_route_lock(route);
	od_list_t *node;
	od_list_foreach (&route->client_pool.active, node) {
		od_client_t *peer =
			od_container_of(node, od_client_t, link_pool);
		od_server_t *server = peer->server;
		if (server == NULL ||
		    server->endpoint != client->server->endpoint) {
			continue;
		}
		uint32_t pid = atomic_load(&server->backend_pid);
		for (size_t i = 0; i < count; i++) {
			if (peer->key.key_pid == virtual_pids[i]) {
				backend_pids[i] = pid;
			}
		}
	}
	od_route_unlock(route);
}

od_frontend_status_t od_isolation_probe_bind(od_client_t *client,
					     const od_pstmt_t *pstmt,
					     machine_msg_t *bind,
					     od_pstmt_t **rewritten_pstmt,
					     machine_msg_t **rewritten_bind)
{
	*rewritten_pstmt = NULL;
	*rewritten_bind = NULL;
	char *pos = machine_msg_data(bind);
	uint32_t rem = machine_msg_size(bind);
	if (kiwi_readn(sizeof(kiwi_header_t), &pos, &rem) == -1) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	char *portal = pos;
	if (kiwi_readsz(&pos, &rem) == -1) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	int portal_len = pos - portal;
	if (kiwi_readsz(&pos, &rem) == -1) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	uint16_t formats, format = 0, params;
	if (kiwi_read16(&formats, &pos, &rem) == -1 || formats > 1 ||
	    (formats && kiwi_read16(&format, &pos, &rem) == -1) || format > 1 ||
	    kiwi_read16(&params, &pos, &rem) == -1 || params != 1) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	uint32_t param_len;
	if (kiwi_read32(&param_len, &pos, &rem) == -1) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	/* Preserve PostgreSQL's handling of NULL and invalid integer values. */
	if (param_len == UINT32_MAX) {
		return OD_OK;
	}
	if (param_len > rem) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	uint32_t victim;
	if (format == 1) {
		if (param_len != 4) {
			return OD_OK;
		}
		char *binary = pos;
		uint32_t size = 4;
		kiwi_read32(&victim, &binary, &size);
	} else {
		const char *text = pos;
		if (!read_pid(&text, pos + param_len, &victim) ||
		    text != pos + param_len) {
			return OD_OK;
		}
	}
	pos += param_len;
	rem -= param_len;
	uint16_t results;
	if (kiwi_read16(&results, &pos, &rem) == -1 || rem != results * 2U) {
		return OD_ECLIENT_PROTOCOL_ERROR;
	}
	char *result_data = pos;
	od_linear_alloc_t *arena = od_worker_get_local_linear_alloc();
	od_linear_alloc_reset(arena, 0);

	const char *query = pstmt->desc.data;
	const char *p = query + pstmt->isolation_pids_offset + 2;
	const char *end = query + pstmt->isolation_pids_offset +
			  pstmt->isolation_pids_len - 2;
	size_t capacity = pstmt->isolation_pids_len / 2 + 1;
	uint32_t *virtual_pids =
		od_linear_alloc_calloc(arena, capacity, sizeof(uint32_t));
	uint32_t *backend_pids =
		od_linear_alloc_calloc(arena, capacity, sizeof(uint32_t));
	if (virtual_pids == NULL || backend_pids == NULL) {
		return OD_EOOM;
	}
	size_t count = 1;
	virtual_pids[0] = victim;
	while (p < end) {
		if (!read_pid(&p, end, &virtual_pids[count++])) {
			return OD_ECLIENT_PROTOCOL_ERROR;
		}
		if (p < end) {
			p++; /* comma, validated when the statement was parsed */
		}
	}
	snapshot_pids(client, virtual_pids, backend_pids, count);
	uint32_t backend_victim = backend_pids[0];

	/* Keep the parameter count/types and SQL result shape unchanged. Only
     * the array literal and the Bind's victim PID change, on every Bind. */
	size_t array_size = count * 12 + 4;
	char *array = od_linear_alloc_alloc(arena, array_size, 0);
	if (array == NULL) {
		return OD_EOOM;
	}
	char *out = array;
	*out++ = '\'';
	*out++ = '{';
	int added = 0;
	for (size_t i = 1; i < count; i++) {
		if (backend_pids[i] == 0) {
			continue; /* unattached sessions cannot hold a lock on this host */
		}
		if (added++) {
			*out++ = ',';
		}
		out += od_snprintf(out, array_size - (out - array), "%u",
				   backend_pids[i]);
	}
	*out++ = '}';
	*out++ = '\'';
	size_t array_len = out - array;
	size_t suffix =
		pstmt->isolation_pids_offset + pstmt->isolation_pids_len;
	od_pstmt_desc_t desc;
	desc.len = pstmt->desc.len - pstmt->isolation_pids_len + array_len;
	desc.data = od_linear_alloc_alloc(arena, desc.len, 0);
	if (desc.data == NULL) {
		return OD_EOOM;
	}
	char *data = desc.data;
	memcpy(data, query, pstmt->isolation_pids_offset);
	memcpy(data + pstmt->isolation_pids_offset, array, array_len);
	memcpy(data + pstmt->isolation_pids_offset + array_len, query + suffix,
	       pstmt->desc.len - suffix);
	od_instance_t *instance = client->global->instance;
	od_pstmt_t *rewritten = od_pstmt_create_or_get(
		instance->pstmts, desc, arena, &instance->config.query_parsing);
	if (rewritten == NULL) {
		return OD_EOOM;
	}
	/* Creating the statement can reset the parser arena. */
	int *result_formats =
		od_linear_alloc_calloc(arena, results, sizeof(int));
	if (results && result_formats == NULL) {
		od_pstmt_unref(rewritten);
		return OD_EOOM;
	}
	for (uint16_t i = 0; i < results; i++) {
		uint16_t result_format;
		kiwi_read16(&result_format, &result_data, &rem);
		result_formats[i] = result_format;
	}
	char pid[12];
	int pid_len = od_snprintf(pid, sizeof(pid), "%u", backend_victim);
	machine_msg_t *msg =
		kiwi_fe_write_bind(NULL, portal, portal_len, rewritten->name,
				   strlen(rewritten->name) + 1, 0, NULL,
				   results, result_formats, 1, &pid_len,
				   (char *[]){ pid });
	if (msg == NULL) {
		od_pstmt_unref(rewritten);
		return OD_EOOM;
	}
	*rewritten_pstmt = rewritten;
	*rewritten_bind = msg;
	return OD_OK;
}

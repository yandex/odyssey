/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <ctype.h>
#include <strings.h>

#include <machinarium/machinarium.h>
#include <kiwi/kiwi.h>

#include <od_memory.h>
#include <util.h>
#include <server.h>
#include <backend.h>
#include <io.h>
#include <pstmt.h>
#include <sql_pstmt.h>
#include <sql/full/parser.h>
#include <worker.h>
#include <instance.h>
#include <global.h>

static const uint64_t hash_seed = 0;

/*
 * name -> entry maps
 */

static mm_hash_t name_hash(const void *key)
{
	const char *name = *(const char **)key;

	return mm_xxh64_hash(name, strlen(name), hash_seed);
}

static int name_cmp(const void *k1, const void *k2)
{
	return strcmp(*(const char **)k1, *(const char **)k2);
}

static void entry_free(od_sql_pstmt_t *e)
{
	od_free(e->name);
	od_free(e->text);
	if (e->pstmt != NULL) {
		od_pstmt_unref(e->pstmt);
	}
	od_free(e);
}

void od_sql_pstmt_map_init(od_sql_pstmt_map_t *map)
{
	map->hm = NULL;
	od_list_init(&map->order);
	map->count = 0;
}

void od_sql_pstmt_map_clear(od_sql_pstmt_map_t *map)
{
	if (map->hm != NULL) {
		mm_hashmap_clear(map->hm);
	}

	od_list_t *i, *n;
	od_list_foreach_safe (&map->order, i, n) {
		od_sql_pstmt_t *e = od_container_of(i, od_sql_pstmt_t, link);
		od_list_unlink(&e->link);
		entry_free(e);
	}

	map->count = 0;
}

void od_sql_pstmt_map_free(od_sql_pstmt_map_t *map)
{
	od_sql_pstmt_map_clear(map);

	if (map->hm != NULL) {
		mm_hashmap_free(map->hm);
		map->hm = NULL;
	}
}

od_sql_pstmt_t *od_sql_pstmt_map_get(od_sql_pstmt_map_t *map, const char *name)
{
	if (map->hm == NULL) {
		return NULL;
	}

	mm_hashmap_keylock_t klock;
	mm_hashmap_lock_key(map->hm, &klock, &name, 0 /* do not create */);
	if (!klock.found) {
		/* lock is not held if key not found */
		return NULL;
	}

	od_sql_pstmt_t *e =
		*(od_sql_pstmt_t **)mm_hashmap_kvp_val(map->hm, klock.kvp);
	mm_hashmap_unlock_key(map->hm, &klock);

	return e;
}

void od_sql_pstmt_map_remove(od_sql_pstmt_map_t *map, const char *name)
{
	if (map->hm == NULL) {
		return;
	}

	mm_hashmap_keylock_t klock;
	mm_hashmap_lock_key(map->hm, &klock, &name, 0 /* do not create */);
	if (!klock.found) {
		return;
	}

	od_sql_pstmt_t *e =
		*(od_sql_pstmt_t **)mm_hashmap_kvp_val(map->hm, klock.kvp);
	mm_hashmap_remove(map->hm, &klock);

	od_list_unlink(&e->link);
	map->count--;
	entry_free(e);
}

od_sql_pstmt_t *od_sql_pstmt_map_set(od_sql_pstmt_map_t *map, const char *name,
				     const char *text, size_t text_len,
				     uint64_t hash)
{
	if (map->hm == NULL) {
		/* maps are owned by one client or server - no locks needed */
		map->hm = mm_hashmap_create(64, 0 /* nlocks */, sizeof(char *),
					    sizeof(od_sql_pstmt_t *), name_cmp,
					    name_hash, NULL /* key dtor */,
					    NULL /* value dtor */,
					    NULL /* key copy */);
		if (map->hm == NULL) {
			return NULL;
		}
	}

	/* the hashmap key points to the entry name, so never reuse an entry */
	od_sql_pstmt_map_remove(map, name);

	od_sql_pstmt_t *e = od_malloc(sizeof(od_sql_pstmt_t));
	if (e == NULL) {
		return NULL;
	}
	memset(e, 0, sizeof(od_sql_pstmt_t));
	od_list_init(&e->link);
	e->hash = hash;

	e->name = od_strdup(name);
	if (e->name == NULL) {
		entry_free(e);
		return NULL;
	}

	if (text != NULL) {
		e->text = od_malloc(text_len + 1);
		if (e->text == NULL) {
			entry_free(e);
			return NULL;
		}
		memcpy(e->text, text, text_len);
		e->text[text_len] = '\0';
		e->text_len = text_len;
	}

	mm_hashmap_keylock_t klock;
	const char *key = e->name;
	if (mm_hashmap_lock_key(map->hm, &klock, &key, MM_HASHMAP_CREATE) ==
	    -1) {
		entry_free(e);
		return NULL;
	}
	memcpy(mm_hashmap_kvp_val(map->hm, klock.kvp), &e, sizeof(e));
	mm_hashmap_unlock_key(map->hm, &klock);

	od_list_append(&map->order, &e->link);
	map->count++;

	return e;
}

void od_sql_pstmt_client_init(od_sql_pstmt_client_t *client)
{
	od_sql_pstmt_map_init(&client->stmts);
	memset(&client->query, 0, sizeof(client->query));
}

void od_sql_pstmt_client_free(od_sql_pstmt_client_t *client)
{
	od_sql_pstmt_map_free(&client->stmts);
	od_sql_pstmt_query_reset(&client->query);
}

void od_sql_pstmt_client_clear(od_sql_pstmt_client_t *client)
{
	od_sql_pstmt_map_clear(&client->stmts);
}

void od_sql_pstmt_server_init(od_sql_pstmt_server_t *server)
{
	od_sql_pstmt_map_init(&server->stmts);
	server->unknown = 0;
}

void od_sql_pstmt_server_free(od_sql_pstmt_server_t *server)
{
	od_sql_pstmt_map_free(&server->stmts);
}

void od_sql_pstmt_server_clear(od_sql_pstmt_server_t *server)
{
	od_sql_pstmt_map_clear(&server->stmts);
}

/*
 * query inspection
 */

/* case-insensitive search of an ASCII keyword */
static int contains_keyword(const char *s, size_t len, const char *kw)
{
	size_t kwlen = strlen(kw);
	char first = kw[0];

	for (size_t i = 0; i + kwlen <= len; ++i) {
		if (tolower((unsigned char)s[i]) != first) {
			continue;
		}
		if (strncasecmp(s + i, kw, kwlen) == 0) {
			return 1;
		}
	}

	return 0;
}

static void ignore_parse_error(const char *msg, void *userdata)
{
	(void)msg;
	(void)userdata;
}

void od_sql_pstmt_op_free(od_sql_pstmt_op_t *op)
{
	od_free(op->name);
	od_free(op->text);
	memset(op, 0, sizeof(od_sql_pstmt_op_t));
}

void od_sql_pstmt_query_reset(od_sql_pstmt_query_t *q)
{
	for (int i = 0; i < q->nops; ++i) {
		od_sql_pstmt_op_free(&q->ops[i]);
	}
	od_free(q->ops);

	memset(q, 0, sizeof(od_sql_pstmt_query_t));
}

/* the prepared statement a statement creates, executes or deallocates */
static const char *stmt_pstmt_name(Node *node)
{
	/* EXPLAIN [ANALYZE] CREATE TABLE ... AS EXECUTE nests two levels */
	while (node != NULL &&
	       (IsA(node, ExplainStmt) || IsA(node, CreateTableAsStmt))) {
		if (IsA(node, ExplainStmt)) {
			node = castNode(ExplainStmt, node)->query;
		} else {
			node = castNode(CreateTableAsStmt, node)->query;
		}
	}

	if (node == NULL) {
		return NULL;
	}

	if (IsA(node, PrepareStmt)) {
		return castNode(PrepareStmt, node)->name;
	}
	if (IsA(node, ExecuteStmt)) {
		return castNode(ExecuteStmt, node)->name;
	}
	if (IsA(node, DeallocateStmt)) {
		return castNode(DeallocateStmt, node)->name;
	}

	return NULL;
}

static int is_reserved_name(const char *name)
{
	return name != NULL && strncmp(name, OD_PSTMT_NAME_PREFIX,
				       sizeof(OD_PSTMT_NAME_PREFIX) - 1) == 0;
}

/*
 * find the prepared statement body, i.e. everything past the
 * "PREPARE name [(types)] AS" prologue, by scanning the statement text.
 * The full SQL parser does not record the body location, and the text is
 * the only source; on anything unexpected NULL is returned and the caller
 * falls back to the full PREPARE text.
 */
typedef struct {
	char *pos;
	char *end;
} prep_scan_t;

static void prep_scan_ws(prep_scan_t *s)
{
	char *pos = s->pos;
	char *end = s->end;

	for (;;) {
		while (pos < end &&
		       (*pos == ' ' || *pos == '\t' || *pos == '\n' ||
			*pos == '\r' || *pos == '\f' || *pos == '\v')) {
			++pos;
		}
		if (end - pos >= 2 && pos[0] == '-' && pos[1] == '-') {
			pos += 2;
			while (pos < end && *pos != '\n') {
				++pos;
			}
			continue;
		}
		if (end - pos >= 2 && pos[0] == '/' && pos[1] == '*') {
			pos += 2;
			while (end - pos >= 2 &&
			       !(pos[0] == '*' && pos[1] == '/')) {
				++pos;
			}
			if (end - pos >= 2) {
				pos += 2;
			} else {
				pos = end;
			}
			continue;
		}
		break;
	}

	s->pos = pos;
}

static int prep_scan_word_is(prep_scan_t *s, const char *word)
{
	size_t len = strlen(word);
	return s->end - s->pos >= (ptrdiff_t)len &&
	       strncasecmp(s->pos, word, len) == 0;
}

static void prep_scan_word(prep_scan_t *s)
{
	char *pos = s->pos;
	if (pos < s->end) {
		++pos;
	}
	while (pos < s->end && (isalnum((unsigned char)*pos) || *pos == '_' ||
				*pos == '$' || *pos == '#')) {
		++pos;
	}
	s->pos = pos;
}

static void prep_scan_quoted(prep_scan_t *s)
{
	char *pos = s->pos;
	char *end = s->end;
	char quote = *pos++;

	while (pos < end) {
		if (*pos == quote) {
			if (pos + 1 < end && pos[1] == quote) {
				pos += 2;
				continue;
			}
			s->pos = pos + 1;
			return;
		}
		++pos;
	}

	s->pos = end;
}

static void prep_scan_parens(prep_scan_t *s)
{
	int depth = 0;

	while (s->pos < s->end) {
		char c = *s->pos;

		if (c == '\'' || c == '"') {
			prep_scan_quoted(s);
			continue;
		}
		if (s->end - s->pos >= 2 && c == '-' && s->pos[1] == '-') {
			while (s->pos < s->end && *s->pos != '\n') {
				++s->pos;
			}
			continue;
		}
		if (s->end - s->pos >= 2 && c == '/' && s->pos[1] == '*') {
			s->pos += 2;
			while (s->end - s->pos >= 2 &&
			       !(s->pos[0] == '*' && s->pos[1] == '/')) {
				++s->pos;
			}
			continue;
		}
		++s->pos;
		if (c == '(') {
			++depth;
		} else if (c == ')') {
			if (--depth == 0) {
				return;
			}
		}
	}
}

static char *prep_body_scan(char *text, size_t text_len, size_t *body_len)
{
	prep_scan_t s = { text, text + text_len };

	/* PREPARE */
	prep_scan_ws(&s);
	if (!prep_scan_word_is(&s, "prepare")) {
		return NULL;
	}
	prep_scan_word(&s);

	/* name */
	prep_scan_ws(&s);
	if (s.pos >= s.end) {
		return NULL;
	}
	if (*s.pos == '"') {
		prep_scan_quoted(&s);
	} else {
		prep_scan_word(&s);
	}

	/* optional (types) */
	prep_scan_ws(&s);
	if (s.pos < s.end && *s.pos == '(') {
		prep_scan_parens(&s);
	}

	/* AS */
	prep_scan_ws(&s);
	if (!prep_scan_word_is(&s, "as")) {
		return NULL;
	}
	prep_scan_word(&s);

	/* the body, without leading whitespace */
	prep_scan_ws(&s);
	if (s.pos >= s.end) {
		return NULL;
	}

	/* and without the trailing whitespace of the query */
	while (s.end > s.pos && isspace((unsigned char)s.end[-1])) {
		--s.end;
	}
	if (s.pos >= s.end) {
		return NULL;
	}

	*body_len = s.end - s.pos;
	return s.pos;
}

static int op_from_stmt(od_sql_pstmt_op_t *op, RawStmt *rs, const char *query,
			size_t len)
{
	Node *node = rs->stmt;

	if (IsA(node, DeallocateStmt)) {
		DeallocateStmt *d = castNode(DeallocateStmt, node);
		if (d->isall) {
			op->type = OD_SQL_PSTMT_OP_DEALLOCATE_ALL;
			return 0;
		}

		op->type = OD_SQL_PSTMT_OP_DEALLOCATE;
		op->name = od_strdup(d->name);
		return op->name == NULL ? -1 : 0;
	}

	PrepareStmt *p = castNode(PrepareStmt, node);
	op->type = OD_SQL_PSTMT_OP_PREPARE;

	/* stmt_len of 0 means the statement runs to the end of the query */
	size_t start = rs->stmt_location > 0 ? (size_t)rs->stmt_location : 0;
	if (start > len) {
		start = len;
	}
	size_t stmt_len = rs->stmt_len > 0 ? (size_t)rs->stmt_len : len - start;
	if (stmt_len > len - start) {
		stmt_len = len - start;
	}

	op->name = od_strdup(p->name);
	op->text = od_malloc(stmt_len + 1);
	if (op->name == NULL || op->text == NULL) {
		return -1;
	}
	memcpy(op->text, query + start, stmt_len);
	op->text[stmt_len] = '\0';
	op->text_len = stmt_len;
	op->hash = mm_xxh64_hash(op->text, stmt_len, hash_seed);

	/*
	 * the prepared statement itself: everything past the "PREPARE name
	 * AS" prologue - this is the text of the statement the clients
	 * share, whatever name it was prepared under
	 */
	op->body = prep_body_scan(op->text, op->text_len, &op->body_len);

	return 0;
}

int od_sql_pstmt_query_analyze(od_sql_pstmt_query_t *q, const char *query,
			       size_t len, od_linear_alloc_t *arena)
{
	q->analyzed = 1;

	/* the worker arena is not allocated when query parsing is disabled */
	if (arena == NULL || arena->buf == NULL) {
		return 0;
	}

	/*
	 * a name in odyssey's namespace either appears literally or is
	 * spelled with escapes in a U&"..." identifier
	 */
	if (!contains_keyword(query, len, "prepare") &&
	    !contains_keyword(query, len, "deallocate") &&
	    !contains_keyword(query, len, OD_PSTMT_NAME_PREFIX) &&
	    !contains_keyword(query, len, "u&")) {
		return 0;
	}

	List *tree =
		od_sql_full_parse(arena, query, len, ignore_parse_error, NULL);
	if (tree == NIL) {
		/* not a valid query or too big for the arena - not tracked */
		return 0;
	}

	int nops = 0;
	ListCell *lc;
	foreach(lc, tree)
	{
		RawStmt *rs = lfirst_node(RawStmt, lc);
		if (is_reserved_name(stmt_pstmt_name(rs->stmt))) {
			/* the query is rejected, no need to track anything */
			q->uses_reserved_name = 1;
			return 0;
		}
		if (IsA(rs->stmt, PrepareStmt) ||
		    IsA(rs->stmt, DeallocateStmt)) {
			nops++;
		}
	}

	if (nops == 0) {
		return 0;
	}

	q->ops = od_malloc(sizeof(od_sql_pstmt_op_t) * nops);
	if (q->ops == NULL) {
		return -1;
	}
	memset(q->ops, 0, sizeof(od_sql_pstmt_op_t) * nops);

	int idx = 0;
	foreach(lc, tree)
	{
		RawStmt *rs = lfirst_node(RawStmt, lc);
		if (IsA(rs->stmt, PrepareStmt) ||
		    IsA(rs->stmt, DeallocateStmt)) {
			od_sql_pstmt_op_t *op = &q->ops[q->nops];
			/* count it before filling, so reset frees partial ops */
			q->nops++;
			op->stmt_idx = idx;
			if (op_from_stmt(op, rs, query, len) != 0) {
				od_sql_pstmt_query_reset(q);
				q->analyzed = 1;
				return -1;
			}
		}
		idx++;
	}

	return 0;
}

int od_sql_pstmt_op_must_forward(od_sql_pstmt_client_t *client,
				 const od_sql_pstmt_op_t *op)
{
	switch (op->type) {
	case OD_SQL_PSTMT_OP_DEALLOCATE:
		return od_sql_pstmt_map_get(&client->stmts, op->name) != NULL;
	case OD_SQL_PSTMT_OP_DEALLOCATE_ALL:
		return client->stmts.count > 0;
	default:
		return 0;
	}
}

int od_sql_pstmt_query_must_forward(od_sql_pstmt_client_t *client)
{
	od_sql_pstmt_query_t *q = &client->query;

	for (int i = 0; i < q->nops; ++i) {
		if (od_sql_pstmt_op_must_forward(client, &q->ops[i])) {
			return 1;
		}
	}

	return 0;
}

static int sql_pstmt_global_register(struct od_global *global,
				     od_sql_pstmt_t *entry,
				     const od_sql_pstmt_op_t *op)
{
	od_instance_t *instance = global->instance;

	/* fall back to the full PREPARE text if the body is unknown */
	const char *desc_data = op->body != NULL ? op->body : entry->text;
	size_t desc_len = op->body != NULL ? op->body_len : entry->text_len;

	od_pstmt_desc_t desc = {
		.data = (char *)desc_data,
		.len = desc_len + 1 /* include the terminating NUL */,
	};

	od_pstmt_t *pstmt =
		od_pstmt_create_or_get(instance->pstmts, desc,
				       od_worker_get_local_linear_alloc(),
				       &instance->config.query_parsing);
	if (pstmt == NULL) {
		return -1;
	}

	/* the entry owns the reference from now on */
	entry->pstmt = pstmt;
	return 0;
}

int od_sql_pstmt_apply(od_sql_pstmt_client_t *client,
		       od_sql_pstmt_server_t *server,
		       const od_sql_pstmt_op_t *op, struct od_global *global)
{
	switch (op->type) {
	case OD_SQL_PSTMT_OP_PREPARE:
		/*
		 * the backend holds the statement already, so if it can not be
		 * recorded the backend state is unknown
		 */
		if (od_sql_pstmt_map_set(&client->stmts, op->name, op->text,
					 op->text_len, op->hash) == NULL) {
			if (server != NULL) {
				server->unknown = 1;
			}
			return -1;
		}
		if (server != NULL &&
		    od_sql_pstmt_map_set(&server->stmts, op->name, NULL, 0,
					 op->hash) == NULL) {
			server->unknown = 1;
			return -1;
		}
		if (global != NULL) {
			od_sql_pstmt_t *entry =
				od_sql_pstmt_map_get(&client->stmts, op->name);
			od_release_assert(entry != NULL);
			if (sql_pstmt_global_register(global, entry, op) != 0) {
				if (server != NULL) {
					server->unknown = 1;
				}
				return -1;
			}
		}
		break;
	case OD_SQL_PSTMT_OP_DEALLOCATE:
		od_sql_pstmt_map_remove(&client->stmts, op->name);
		if (server != NULL) {
			od_sql_pstmt_map_remove(&server->stmts, op->name);
		}
		break;
	case OD_SQL_PSTMT_OP_DEALLOCATE_ALL:
		od_sql_pstmt_client_clear(client);
		if (server != NULL) {
			od_sql_pstmt_server_clear(server);
		}
		break;
	}

	return 0;
}

int od_sql_pstmt_query_finish(od_sql_pstmt_client_t *client,
			      od_sql_pstmt_server_t *server,
			      int *deallocated_all, struct od_global *global)
{
	od_sql_pstmt_query_t *q = &client->query;

	*deallocated_all = 0;

	if (!q->counting) {
		return 0;
	}
	q->counting = 0;

	for (int i = 0; i < q->nops; ++i) {
		od_sql_pstmt_op_t *op = &q->ops[i];

		/*
		 * statements run in order and execution stops at the first
		 * error, so statement N succeeded iff more than N completed
		 */
		if (op->stmt_idx >= q->completes) {
			break;
		}

		if (od_sql_pstmt_apply(client, server, op, global) != 0) {
			return -1;
		}

		if (op->type == OD_SQL_PSTMT_OP_DEALLOCATE_ALL) {
			*deallocated_all = 1;
		}
	}

	return 0;
}

/*
 * attach
 */

static machine_msg_t *write_deallocate(machine_msg_t *stream, const char *name)
{
	/* DEALLOCATE "name" with embedded quotes doubled */
	size_t len = strlen(name);
	char *query = od_malloc(sizeof("DEALLOCATE \"\"") + 2 * len);
	if (query == NULL) {
		/* stream is NULL for the first query of a batch */
		machine_msg_free_safe(stream);
		return NULL;
	}

	char *pos = query;
	pos += sprintf(pos, "DEALLOCATE \"");
	for (size_t i = 0; i < len; ++i) {
		if (name[i] == '"') {
			*pos++ = '"';
		}
		*pos++ = name[i];
	}
	*pos++ = '"';
	*pos++ = '\0';

	machine_msg_t *msg = kiwi_fe_write_query(stream, query, pos - query);
	od_free(query);
	if (msg == NULL) {
		/* on failure the stream is left to the caller */
		machine_msg_free_safe(stream);
	}

	return msg;
}

/* SQLSTATE of a failed service query, empty if it succeeded */
typedef char sqlstate_t[6];

/*
 * sends count pipelined queries and waits for all of them, ignoring
 * errors; errors[i] is the SQLSTATE of query i, empty if it succeeded
 */
static int run_service_queries(od_server_t *server, char *context,
			       machine_msg_t *stream, int count,
			       sqlstate_t *errors, uint32_t timeout_ms)
{
	if (od_write2(&server->io, stream, timeout_ms) == -1) {
		return -1;
	}

	od_server_sync_request(server, count);

	for (int i = 0; i < count; ++i) {
		errors[i][0] = '\0';

		for (;;) {
			machine_msg_t *msg =
				od_read(&server->io, timeout_ms, OD_READ_BE);
			if (msg == NULL) {
				return -1;
			}

			char *data = machine_msg_data(msg);
			uint32_t size = machine_msg_size(msg);
			kiwi_be_type_t type = *data;

			if (type == KIWI_BE_ERROR_RESPONSE) {
				/* logs the error and updates server flags */
				od_backend_error(server, context, data, size);

				kiwi_fe_error_t error;
				if (kiwi_fe_read_error(data, size, &error) ==
				    0) {
					od_snprintf(errors[i],
						    sizeof(sqlstate_t), "%s",
						    error.code);
				} else {
					od_snprintf(errors[i],
						    sizeof(sqlstate_t), "?");
				}
			} else if (type == KIWI_BE_READY_FOR_QUERY) {
				int rc = od_backend_ready(server, data, size);
				machine_msg_free(msg);
				if (rc == -1) {
					return -1;
				}
				break;
			}

			machine_msg_free(msg);
		}
	}

	return 0;
}

typedef struct {
	od_sql_pstmt_t *entry;
	/* deallocate a server entry, otherwise prepare a client entry */
	int is_deallocate;
} attach_step_t;

int od_sql_pstmt_attach(od_sql_pstmt_client_t *client,
			od_sql_pstmt_server_t *srv, od_server_t *server,
			char *context, uint32_t timeout_ms)
{
	if (srv->unknown) {
		return 1;
	}

	if (client->stmts.count == 0 && srv->stmts.count == 0) {
		return 0;
	}

	size_t max_steps = client->stmts.count + srv->stmts.count;
	attach_step_t *steps = od_malloc(sizeof(attach_step_t) * max_steps);
	sqlstate_t *errors = od_malloc(sizeof(sqlstate_t) * max_steps);
	machine_msg_t *stream = NULL;
	int count = 0;
	int rc = -1;

	if (steps == NULL || errors == NULL) {
		goto out;
	}

	od_list_t *i;

	/* statements of other clients, or of this client with other text */
	od_list_foreach (&srv->stmts.order, i) {
		od_sql_pstmt_t *held = od_container_of(i, od_sql_pstmt_t, link);
		od_sql_pstmt_t *own =
			od_sql_pstmt_map_get(&client->stmts, held->name);

		if (own != NULL && own->hash == held->hash) {
			continue;
		}

		stream = write_deallocate(stream, held->name);
		if (stream == NULL) {
			goto out;
		}
		steps[count++] = (attach_step_t){ held, 1 };
	}

	/* statements of this client the backend does not hold */
	od_list_foreach (&client->stmts.order, i) {
		od_sql_pstmt_t *own = od_container_of(i, od_sql_pstmt_t, link);
		od_sql_pstmt_t *held =
			od_sql_pstmt_map_get(&srv->stmts, own->name);

		if (held != NULL && held->hash == own->hash) {
			continue;
		}

		machine_msg_t *msg = kiwi_fe_write_query(stream, own->text,
							 own->text_len + 1);
		if (msg == NULL) {
			goto out;
		}
		stream = msg;
		steps[count++] = (attach_step_t){ own, 0 };

		od_gdebug(context, server->client, server,
			  "deploy prepared statement \"%s\"", own->name);
	}

	if (count == 0) {
		rc = 0;
		goto out;
	}

	rc = run_service_queries(server, context, stream, count, errors,
				 timeout_ms);
	stream = NULL;
	if (rc != 0) {
		goto out;
	}

	/* deallocations go first, so a name is freed before it is prepared */
	int unknown_state = 0;
	for (int s = 0; s < count; ++s) {
		od_sql_pstmt_t *e = steps[s].entry;
		const char *code = errors[s];

		if (steps[s].is_deallocate) {
			if (code[0] == '\0' ||
			    strcmp(code, KIWI_INVALID_SQL_STATEMENT_NAME) ==
				    0) {
				/* deallocated now, or already gone */
				od_sql_pstmt_map_remove(&srv->stmts, e->name);
			} else {
				od_glog(context, server->client, server,
					"failed to deallocate prepared statement \"%s\" (%s)",
					e->name, code);
				unknown_state = 1;
			}
			continue;
		}

		if (code[0] == '\0') {
			if (od_sql_pstmt_map_set(&srv->stmts, e->name, NULL, 0,
						 e->hash) == NULL) {
				srv->unknown = 1;
				rc = -1;
				goto out;
			}
			continue;
		}

		if (strcmp(code, KIWI_DUPLICATE_PSTATEMENT) == 0) {
			/*
			 * the backend holds a statement under this name that
			 * odyssey does not know about
			 */
			od_glog(context, server->client, server,
				"prepared statement \"%s\" already exists on the server",
				e->name);
			unknown_state = 1;
			continue;
		}

		/*
		 * the statement can not be recreated (e.g. a referenced
		 * table was dropped); forget it, so later EXECUTE reports
		 * that it does not exist instead of retrying on every attach
		 */
		od_glog(context, server->client, server,
			"failed to restore prepared statement \"%s\" on the server (%s), dropping it",
			e->name, code);
		od_sql_pstmt_map_remove(&client->stmts, e->name);
	}

	if (unknown_state) {
		srv->unknown = 1;
		rc = 1;
	}

out:
	if (stream != NULL) {
		machine_msg_free(stream);
	}
	od_free(steps);
	od_free(errors);

	return rc;
}

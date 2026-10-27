/*
 * Odyssey.
 *
 * SQL PREPARE creates ordinary protocol-level cached statements.
 * Locations come from the minimal parser; PostgreSQL validates their bodies.
 */
#include <odyssey.h>
#include <kiwi/kiwi.h>
#include <client.h>
#include <server.h>
#include <global.h>
#include <instance.h>
#include <worker.h>
#include <backend.h>
#include <stream.h>
#include <sql_pstmt.h>
#include <wait_event.h>
#include <util.h>
#include <sql/minimal/parser.h>
#include <sql/full/parser.h>
#include <ctype.h>
#include <strings.h>

static char *copy_text(const char *text, od_sql_location_t location)
{
	int length = location.end - location.start;
	char *copy = od_malloc((size_t)length + 1);
	if (copy != NULL) {
		memcpy(copy, text + location.start, (size_t)length);
		copy[length] = 0;
	}
	return copy;
}

static void ignore_parse_error(const char *message, void *arg)
{
	(void)message;
	(void)arg;
}

static const char *statement_name(Node *node)
{
	while (node != NULL &&
	       (IsA(node, ExplainStmt) || IsA(node, CreateTableAsStmt))) {
		node = IsA(node, ExplainStmt) ?
			       castNode(ExplainStmt, node)->query :
			       castNode(CreateTableAsStmt, node)->query;
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

/* Skip parsing ordinary traffic that cannot affect prepared statements. */
static int contains_keyword(const char *text, size_t len, const char *word)
{
	size_t length = strlen(word);
	for (size_t i = 0; i + length <= len; i++) {
		if (tolower((unsigned char)text[i]) == word[0] &&
		    strncasecmp(text + i, word, length) == 0) {
			return 1;
		}
	}
	return 0;
}

void od_sql_pstmt_query_reset(od_sql_pstmt_query_t *q)
{
	for (int i = 0; i < q->nops; i++) {
		od_sql_pstmt_op_t *op = &q->ops[i];
		od_free(op->name);
		od_free(op->query);
		for (int j = 0; j < op->nargs; j++) {
			od_free(op->args[j]);
		}
		od_free(op->args);
		od_free(op->arg_lengths);
		if (op->pstmt != NULL) {
			od_pstmt_unref(op->pstmt);
		}
	}
	od_free(q->ops);
	od_free(q->text);
	machine_msg_free_safe(q->rewritten);
	memset(q, 0, sizeof(*q));
}

int od_sql_pstmt_query_analyze(od_sql_pstmt_query_t *q, const char *text,
			       size_t len, od_linear_alloc_t *arena)
{
	q->analyzed = 1;
	if (arena == NULL || arena->buf == NULL) {
		return 0;
	}
	if (!contains_keyword(text, len, "prepare") &&
	    !contains_keyword(text, len, "execute") &&
	    !contains_keyword(text, len, "deallocate") &&
	    !contains_keyword(text, len, "discard")) {
		return 0;
	}

	/* Full grammar checks the batch; the minimal parser supplies byte ranges. */
	List *tree =
		od_sql_full_parse(arena, text, len, ignore_parse_error, NULL);
	ListCell *cell;
	foreach(cell, tree)
	{
		Node *node = lfirst_node(RawStmt, cell)->stmt;
		const char *name = statement_name(node);
		if (name != NULL &&
		    strncmp(name, OD_PSTMT_NAME_PREFIX,
			    sizeof(OD_PSTMT_NAME_PREFIX) - 1) == 0) {
			q->uses_reserved_name = 1;
			return 0;
		}
		if (IsA(node, PrepareStmt) &&
		    castNode(PrepareStmt, node)->argtypes != NIL) {
			q->unsupported = 1;
		}
		if (name != NULL && !IsA(node, PrepareStmt) &&
		    !IsA(node, ExecuteStmt) && !IsA(node, DeallocateStmt)) {
			q->unsupported = 1;
		}
	}
	if (tree == NIL || q->unsupported) {
		return 0;
	}

	q->nops = list_length(tree);
	q->ops = od_malloc(sizeof(*q->ops) * (size_t)q->nops);
	q->text = od_malloc(len + 1);
	if (q->ops == NULL || q->text == NULL) {
		od_free(q->ops);
		q->ops = NULL;
		q->nops = 0;
		return -1;
	}
	memset(q->ops, 0, sizeof(*q->ops) * (size_t)q->nops);
	memcpy(q->text, text, len);
	q->text[len] = 0;

	/* Copy full-parser metadata before the minimal parser resets the arena. */
	int index = 0;
	int has_copy = 0;
	foreach(cell, tree)
	{
		RawStmt *raw = lfirst_node(RawStmt, cell);
		has_copy |= IsA(raw->stmt, CopyStmt);
		od_sql_pstmt_op_t *op = &q->ops[index++];
		op->location =
			(od_sql_location_t){ raw->stmt_location,
					     raw->stmt_len != 0 ?
						     raw->stmt_location +
							     raw->stmt_len :
						     (int)len };
		if (IsA(raw->stmt, PrepareStmt)) {
			op->type = OD_SQL_PSTMT_PREPARE;
		} else if (IsA(raw->stmt, ExecuteStmt)) {
			op->type = OD_SQL_PSTMT_EXECUTE;
			List *params = castNode(ExecuteStmt, raw->stmt)->params;
			int nargs = list_length(params);
			if (nargs > UINT16_MAX) {
				q->unsupported = 1;
				return 0;
			}
			if (nargs > 0) {
				op->args = od_malloc(sizeof(*op->args) *
						     (size_t)nargs);
				if (op->args == NULL) {
					return -1;
				}
				memset(op->args, 0,
				       sizeof(*op->args) * (size_t)nargs);
				op->nargs = nargs;
				op->arg_lengths =
					od_malloc(sizeof(*op->arg_lengths) *
						  (size_t)nargs);
				if (op->arg_lengths == NULL) {
					return -1;
				}
			}
			ListCell *param;
			int i = 0;
			foreach(param, params)
			{
				Node *value = lfirst(param);
				if (!IsA(value, A_Const)) {
					q->unsupported = 1;
					return 0;
				}
				A_Const *constant = castNode(A_Const, value);
				if (constant->isnull) {
					op->arg_lengths[i++] = -1;
					continue;
				}
				char number[32];
				const char *text;
				value = &constant->val.node;
				switch (nodeTag(value)) {
				case T_Integer:
					od_snprintf(number, sizeof(number),
						    "%d", intVal(value));
					text = number;
					break;
				case T_Float:
					text = floatVal(value);
					break;
				case T_String:
					text = strVal(value);
					break;
				case T_Boolean:
					text = boolVal(value) ? "true" :
								"false";
					break;
				default:
					q->unsupported = 1;
					return 0;
				}
				op->args[i] = od_strdup(text);
				if (op->args[i] == NULL) {
					return -1;
				}
				op->arg_lengths[i++] = (int)strlen(text);
			}
		} else if (IsA(raw->stmt, DeallocateStmt)) {
			op->type = OD_SQL_PSTMT_DEALLOCATE;
		}
	}
	for (int i = 0; i < q->nops; i++) {
		od_sql_pstmt_op_t *op = &q->ops[i];
		od_sql_minimal_node_t *node = od_sql_minimal_parse(
			text + op->location.start,
			(size_t)(op->location.end - op->location.start), arena,
			NULL, NULL);
		if (node == NULL) {
			if (op->type != OD_SQL_PSTMT_OTHER) {
				q->unsupported = 1;
				break;
			}
			continue;
		}
		const char *name = NULL;
		if (node->type == OD_SQL_MINIMAL_NODE_TYPE_PREPARE_STMT) {
			od_sql_minimal_prepare_stmt_t *prepare = (void *)node;
			name = prepare->name;
			op->name_location = prepare->name_location;
			op->query_location = prepare->query_location;
			op->query_location.start += op->location.start;
			op->query_location.end += op->location.start;
		} else if (node->type ==
			   OD_SQL_MINIMAL_NODE_TYPE_EXECUTE_STMT) {
			od_sql_minimal_execute_stmt_t *execute = (void *)node;
			name = execute->name;
			op->name_location = execute->name_location;
		} else if (node->type ==
			   OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT) {
			od_sql_minimal_deallocate_stmt_t *deallocate =
				(void *)node;
			op->type = deallocate->is_all ?
					   OD_SQL_PSTMT_DEALLOCATE_ALL :
					   OD_SQL_PSTMT_DEALLOCATE;
			name = deallocate->name;
		} else if (node->type ==
				   OD_SQL_MINIMAL_NODE_TYPE_DISCARD_STMT &&
			   ((od_sql_minimal_discard_stmt_t *)node)->target ==
				   OD_SQL_MINIMAL_DISCARD_ALL) {
			op->type = OD_SQL_PSTMT_DISCARD_ALL;
		}
		if (name != NULL) {
			/* PostgreSQL truncates longer identifiers; this subset rejects them. */
			if (name[0] == 0 || strlen(name) >= OD_PG_NAMEDATALEN) {
				q->unsupported = 1;
				break;
			}
			op->name = od_strdup(name);
			if (op->name == NULL) {
				return -1;
			}
			op->name_location.start += op->location.start;
			op->name_location.end += op->location.start;
		}
		q->active |= op->type != OD_SQL_PSTMT_OTHER;
	}
	/* COPY IN needs a client-driven Sync, unlike this complete batch. */
	if (q->active && has_copy) {
		q->unsupported = 1;
	}
	return 0;
}

static od_pstmt_t *client_statement(od_client_t *client, int before,
				    const char *name)
{
	od_sql_pstmt_query_t *q = &client->sql_query;
	for (int i = before - 1; i >= 0; i--) {
		od_sql_pstmt_op_t *op = &q->ops[i];
		if (op->type == OD_SQL_PSTMT_DEALLOCATE_ALL ||
		    op->type == OD_SQL_PSTMT_DISCARD_ALL) {
			return NULL;
		}
		if (op->name == NULL || strcmp(name, op->name) != 0) {
			continue;
		}
		if (op->type == OD_SQL_PSTMT_DEALLOCATE) {
			return NULL;
		}
		if (op->type == OD_SQL_PSTMT_PREPARE) {
			return op->pstmt;
		}
	}
	return client->prep_stmt_ids != NULL ?
		       od_client_get_pstmt(client, name) :
		       NULL;
}

static int prepared_before(od_sql_pstmt_query_t *q, int before,
			   od_server_t *server, od_pstmt_t *pstmt)
{
	for (int i = before - 1; i >= 0; i--) {
		od_sql_pstmt_op_t *op = &q->ops[i];
		if (op->type == OD_SQL_PSTMT_DISCARD_ALL) {
			return 0;
		}
		if ((op->type == OD_SQL_PSTMT_PREPARE ||
		     op->type == OD_SQL_PSTMT_EXECUTE) &&
		    op->pstmt == pstmt) {
			return 1;
		}
	}
	return od_server_has_pstmt(server, pstmt);
}

/* Use the same Parse representation as protocol-created cached statements. */
static int append_parse(machine_msg_t *out, od_sql_pstmt_op_t *op,
			od_pstmt_t *pstmt)
{
	machine_msg_t *parse = od_pstmt_parse_of(pstmt);
	if (parse == NULL) {
		return -1;
	}
	int rc = machine_msg_write(out, machine_msg_data(parse),
				   machine_msg_size(parse));
	machine_msg_free(parse);
	if (rc == 0) {
		op->parses++;
	}
	return rc;
}

static int append_bind(machine_msg_t **out, char *name, int nargs, int *lengths,
		       char **args)
{
	machine_msg_t *next =
		kiwi_fe_write_bind(*out, "", 1, name, (int)strlen(name) + 1, 0,
				   NULL, 0, NULL, nargs, lengths, args);
	if (next == NULL) {
		return -1;
	}
	*out = next;
	next = kiwi_fe_write_describe(*out, 'P', "", 1);
	if (next == NULL) {
		return -1;
	}
	*out = next;
	next = kiwi_fe_write_execute(*out, "", 1, 0);
	if (next == NULL) {
		return -1;
	}
	*out = next;
	return 0;
}

/* Execute one ordinary statement without ending the surrounding batch. */
static int append_command(machine_msg_t **out, const char *query)
{
	machine_msg_t *next = kiwi_fe_write_parse(
		*out, "", 1, query, (int)strlen(query) + 1, 0, NULL);
	if (next == NULL) {
		return -1;
	}
	*out = next;
	return append_bind(out, "", 0, NULL, NULL);
}

/* Force the usual backend error, including transaction abort, without SQL.
 * This nonnumeric reserved name is never used by the statement cache. */
static int append_missing(machine_msg_t **out)
{
	char *name = OD_PSTMT_NAME_PREFIX "missing";
	machine_msg_t *next =
		kiwi_fe_write_close(*out, 'S', name, (int)strlen(name) + 1);
	if (next == NULL) {
		return -1;
	}
	*out = next;
	return append_bind(out, name, 0, NULL, NULL);
}

od_frontend_status_t od_sql_pstmt_query_rewrite(od_client_t *client)
{
	od_sql_pstmt_query_t *q = &client->sql_query;
	if (!q->active || q->rewritten != NULL) {
		return OD_OK;
	}
	od_server_t *server = client->server;
	od_instance_t *instance = client->global->instance;
	machine_msg_t *out = machine_msg_create(0);
	if (out == NULL) {
		return OD_EOOM;
	}
	q->pending_begin = client->pending_begin;
	if (q->pending_begin) {
		if (append_command(&out, "BEGIN") != 0) {
			goto oom;
		}
	}
	for (int i = 0; i < q->nops; i++) {
		od_sql_pstmt_op_t *op = &q->ops[i];
		od_pstmt_t *own =
			op->name != NULL ?
				client_statement(client, i, op->name) :
				NULL;
		if (op->type == OD_SQL_PSTMT_PREPARE) {
			if (own != NULL) {
				op->pstmt = own;
				od_pstmt_ref(own);
			} else {
				char *body =
					copy_text(q->text, op->query_location);
				if (body == NULL) {
					goto oom;
				}
				size_t length = strlen(body) + 1;
				char *bytes = od_malloc(length + 2);
				if (bytes == NULL) {
					od_free(body);
					goto oom;
				}
				memcpy(bytes, body, length);
				bytes[length] = bytes[length + 1] = 0;
				od_free(body);
				od_pstmt_desc_t desc = {
					.data = bytes,
					.len = length + 2,
				};
				op->pstmt = od_pstmt_create_or_get(
					od_instance_get_pstmts_map(instance),
					desc,
					od_worker_get_local_linear_alloc(),
					&instance->config.query_parsing);
				od_free(bytes);
				if (op->pstmt == NULL) {
					goto oom;
				}
			}
			int prepared = prepared_before(q, i, server, op->pstmt);
			if (own == NULL && prepared) {
				op->dummy = 1;
			} else {
				/* Recreate an existing alias first so a duplicate still fails. */
				if (own != NULL && !prepared &&
				    append_parse(out, op, own) != 0) {
					goto oom;
				}
				if (append_parse(out, op, op->pstmt) != 0) {
					goto oom;
				}
				op->parse_body = 1;
				op->dummy = 1;
				op->query =
					copy_text(q->text, op->query_location);
				if (op->query == NULL) {
					goto oom;
				}
			}
		} else if (op->type == OD_SQL_PSTMT_EXECUTE) {
			if (own == NULL) {
				if (append_missing(&out) != 0) {
					goto oom;
				}
			} else {
				op->pstmt = own;
				od_pstmt_ref(own);
				if ((!prepared_before(q, i, server, own) &&
				     append_parse(out, op, own) != 0) ||
				    append_bind(&out, own->name, op->nargs,
						op->arg_lengths,
						op->args) != 0) {
					goto oom;
				}
			}
			continue;
		} else if (op->type == OD_SQL_PSTMT_DEALLOCATE_ALL ||
			   (op->type == OD_SQL_PSTMT_DEALLOCATE &&
			    own != NULL)) {
			op->dummy = 1;
		} else if (op->type == OD_SQL_PSTMT_DEALLOCATE) {
			if (append_missing(&out) != 0) {
				goto oom;
			}
			continue;
		}
		if (op->dummy && !op->parse_body) {
			op->query = od_strdup("SELECT WHERE false");
		} else if (!op->parse_body) {
			op->query = copy_text(q->text, op->location);
		}
		if (op->query == NULL) {
			goto oom;
		}
		/* Execute a marker so PREPARE has ordinary batch transaction semantics. */
		if (append_command(&out, op->dummy ? "SELECT WHERE false" :
						     op->query) != 0) {
			goto oom;
		}
		op->parses++;
	}
	q->rewritten = kiwi_fe_write_sync(out);
	if (q->rewritten == NULL) {
		goto oom;
	}
	return OD_OK;
oom:
	machine_msg_free(out);
	return OD_EOOM;
}

static int original_position(od_sql_pstmt_query_t *q, od_sql_pstmt_op_t *op,
			     int cursor)
{
	int byte = op->name_location.start;
	if (op->query != NULL &&
	    !(op->parses > 1 && (!op->parse_body || op->parses > 2))) {
		const unsigned char *query = (const unsigned char *)op->query;
		byte = 0;
		for (int character = 1; query[byte] && character < cursor;) {
			character++;
			do {
				byte++;
			} while ((query[byte] & 0xc0) == 0x80);
		}
		byte += op->parse_body ? op->query_location.start :
					 op->location.start;
	}
	int character = 1;
	for (int i = 0; i < byte; i++) {
		if (((unsigned char)q->text[i] & 0xc0) != 0x80) {
			character++;
		}
	}
	return character;
}

static machine_msg_t *rewrite_error(od_sql_pstmt_query_t *q,
				    od_sql_pstmt_op_t *op,
				    machine_msg_t *message)
{
	char *data = machine_msg_data(message);
	uint32_t size = (uint32_t)machine_msg_size(message);
	kiwi_fe_error_t error;
	if (kiwi_fe_read_error(data, size, &error) != 0) {
		return NULL;
	}
	machine_msg_t *out = machine_msg_create(sizeof(kiwi_header_t));
	if (out == NULL) {
		return NULL;
	}
	char replacement[OD_QRY_MAX_SZ];
	size_t position = sizeof(kiwi_header_t);
	while (position < size && data[position] != 0) {
		char kind = data[position++];
		size_t left = size - position;
		size_t length = strnlen(data + position, left);
		if (length == left) {
			goto error;
		}
		char *value = data + position;
		if (kind == 'P' && op != NULL) {
			od_snprintf(replacement, sizeof(replacement), "%d",
				    original_position(q, op, atoi(value)));
			value = replacement;
		} else if (kind == 'M' && op != NULL && op->name != NULL &&
			   (strcmp(error.code, KIWI_DUPLICATE_PSTATEMENT) ==
				    0 ||
			    strcmp(error.code,
				   KIWI_INVALID_SQL_STATEMENT_NAME) == 0)) {
			od_snprintf(replacement, sizeof(replacement),
				    "prepared statement \"%s\" %s", op->name,
				    strcmp(error.code,
					   KIWI_DUPLICATE_PSTATEMENT) == 0 ?
					    "already exists" :
					    "does not exist");
			value = replacement;
		} else if (kind == 'M' && op != NULL && op->pstmt != NULL &&
			   strcmp(error.code, KIWI_PROTOCOL_VIOLATION) == 0) {
			char *name = strstr(value, op->pstmt->name);
			if (name != NULL) {
				od_snprintf(replacement, sizeof(replacement),
					    "%.*s%s%s", (int)(name - value),
					    value, op->name,
					    name + strlen(op->pstmt->name));
				value = replacement;
			}
		}
		if (machine_msg_write(out, &kind, 1) != 0 ||
		    machine_msg_write(out, value, (int)strlen(value) + 1) !=
			    0) {
			goto error;
		}
		position += length + 1;
	}
	if (machine_msg_write(out, "", 1) != 0) {
		goto error;
	}
	kiwi_header_t *header = machine_msg_data(out);
	header->type = KIWI_BE_ERROR_RESPONSE;
	kiwi_header_set_size(header, machine_msg_size(out));
	return out;
error:
	machine_msg_free(out);
	return NULL;
}

od_frontend_status_t od_sql_pstmt_query_response(od_client_t *client,
						 machine_msg_t **message)
{
	od_sql_pstmt_query_t *q = &client->sql_query;
	od_sql_pstmt_op_t *op =
		q->completes < q->nops ? &q->ops[q->completes] : NULL;
	kiwi_be_type_t type = *(uint8_t *)machine_msg_data(*message);
	machine_msg_t *replacement = NULL;
	if (q->pending_begin) {
		if (type == KIWI_BE_COMMAND_COMPLETE) {
			q->pending_begin = 0;
			machine_msg_free(*message);
			*message = NULL;
			return OD_OK;
		}
		if (type == KIWI_BE_PARSE_COMPLETE ||
		    type == KIWI_BE_BIND_COMPLETE || type == KIWI_BE_NO_DATA) {
			machine_msg_free(*message);
			*message = NULL;
			return OD_OK;
		}
	} else if (type == KIWI_BE_PARSE_COMPLETE) {
		if (op == NULL || op->parses == 0) {
			return OD_ESERVER_READ;
		}
		if ((op->parses > 1 || op->type == OD_SQL_PSTMT_EXECUTE) &&
		    od_server_add_pstmt(client->server, op->pstmt) < 0) {
			return OD_EOOM;
		}
		if (op->parse_body && op->parses == 2) {
			op->completed = 1;
		}
		op->parses--;
		machine_msg_free(*message);
		*message = NULL;
		return OD_OK;
	} else if (type == KIWI_BE_BIND_COMPLETE || type == KIWI_BE_NO_DATA ||
		   type == KIWI_BE_CLOSE_COMPLETE) {
		machine_msg_free(*message);
		*message = NULL;
		return OD_OK;
	}
	if (op != NULL && op->dummy && type == KIWI_BE_ROW_DESCRIPTION) {
		machine_msg_free(*message);
		*message = NULL;
		return OD_OK;
	}
	if (type == KIWI_BE_COMMAND_COMPLETE) {
		if (op == NULL) {
			return OD_ESERVER_READ;
		}
		op->completed = 1;
		q->completes++;
		if (op->dummy) {
			const char *tag =
				op->type == OD_SQL_PSTMT_PREPARE ?
					"PREPARE" :
				op->type == OD_SQL_PSTMT_DEALLOCATE_ALL ?
					"DEALLOCATE ALL" :
					"DEALLOCATE";
			replacement = kiwi_be_write_command_complete(NULL, tag);
			if (replacement == NULL) {
				return OD_EOOM;
			}
		}
	} else if (type == KIWI_BE_ERROR_RESPONSE) {
		replacement = rewrite_error(q, op, *message);
		if (replacement == NULL) {
			return OD_EOOM;
		}
	}
	if (replacement != NULL) {
		machine_msg_free(*message);
		*message = replacement;
	}
	return OD_OK;
}

od_frontend_status_t od_sql_pstmt_query_finish(od_client_t *client)
{
	od_sql_pstmt_query_t *q = &client->sql_query;
	od_server_t *server = client->server;
	for (int i = 0; i < q->nops; i++) {
		od_sql_pstmt_op_t *op = &q->ops[i];
		if (!op->completed) {
			break;
		}
		switch (op->type) {
		case OD_SQL_PSTMT_PREPARE:
			if (od_client_add_pstmt(client, op->name, op->pstmt) !=
				    0 ||
			    od_server_add_pstmt(server, op->pstmt) < 0) {
				return OD_EOOM;
			}
			break;
		case OD_SQL_PSTMT_DEALLOCATE:
			od_client_remove_pstmt(client, op->name);
			break;
		case OD_SQL_PSTMT_DEALLOCATE_ALL:
			od_client_pstmts_clear(client);
			od_client_portals_clear(client);
			break;
		case OD_SQL_PSTMT_DISCARD_ALL:
			od_client_pstmts_clear(client);
			od_client_portals_clear(client);
			od_server_pstmts_clear(server);
			break;
		default:
			break;
		}
	}
	return OD_OK;
}

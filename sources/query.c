
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <query.h>
#include <server.h>
#include <global.h>
#include <instance.h>
#include <backend.h>
#include <util.h>
#include <sql/minimal/parser.h>
#include <sql/full/parser.h>
#include <sql/full/pg_constants.h>

machine_msg_t *od_query_do(od_server_t *server, char *context,
			   const char *query, char *param, uint32_t timeout_ms)
{
	od_instance_t *instance = server->global->instance;
	od_debug(&instance->logger, context, server->client, server, "%s",
		 query);

	if (od_backend_query_send(server, context, query, param,
				  strlen(query) + 1,
				  timeout_ms) == NOT_OK_RESPONSE) {
		return NULL;
	}
	machine_msg_t *ret_msg = NULL;
	machine_msg_t *msg;

	/* wait for response */
	int has_result = 0;
	for (;;) {
		msg = od_read(&server->io, timeout_ms, OD_READ_BE);
		if (msg == NULL) {
			if (!machine_timedout()) {
				od_error(&instance->logger, context,
					 server->client, server,
					 "read error: %s",
					 od_io_error(&server->io));
			}
			if (ret_msg) {
				machine_msg_free(ret_msg);
			}
			return NULL;
		}

		int save_msg = 0;
		kiwi_be_type_t type;
		type = *(char *)machine_msg_data(msg);

		od_debug(&instance->logger, context, server->client, server,
			 "%s", kiwi_be_type_to_string(type));

		switch (type) {
		case KIWI_BE_ERROR_RESPONSE:
			od_backend_error(server, context, machine_msg_data(msg),
					 machine_msg_size(msg));
			goto error;
		case KIWI_BE_ROW_DESCRIPTION:
			break;
		case KIWI_BE_DATA_ROW: {
			if (has_result) {
				goto error;
			}

			ret_msg = msg;
			has_result = 1;
			save_msg = 1;
			break;
		}
		case KIWI_BE_READY_FOR_QUERY:
			od_backend_ready(server, machine_msg_data(msg),
					 machine_msg_size(msg));

			machine_msg_free(msg);
			return ret_msg;
		default:
			break;
		}

		if (!save_msg) {
			machine_msg_free(msg);
		}
	}
	return ret_msg;
error:
	machine_msg_free(msg);
	if (ret_msg) {
		machine_msg_free(ret_msg);
	}
	return NULL;
}

__attribute__((hot)) int od_query_format(char *format_pos, char *format_end,
					 kiwi_var_t *user, char *peer,
					 char *output, int output_len)
{
	char *dst_pos = output;
	char *dst_end = output + output_len;
	while (format_pos < format_end) {
		if (*format_pos == '%') {
			format_pos++;
			if (od_unlikely(format_pos == format_end)) {
				break;
			}
			int len;
			switch (*format_pos) {
			case 'u':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%s", user->value);
				dst_pos += len;
				break;
			case 'h':
				len = od_snprintf(dst_pos, dst_end - dst_pos,
						  "%s", peer);
				dst_pos += len;
				break;
			default:
				if (od_unlikely((dst_end - dst_pos) < 2)) {
					break;
				}
				dst_pos[0] = '%';
				dst_pos[1] = *format_pos;
				dst_pos += 2;
				break;
			}
		} else {
			if (od_unlikely((dst_end - dst_pos) < 1)) {
				break;
			}
			dst_pos[0] = *format_pos;
			dst_pos += 1;
		}
		format_pos++;
	}
	if (od_unlikely((dst_end - dst_pos) < 1)) {
		return -1;
	}
	dst_pos[0] = 0;
	dst_pos++;
	return dst_pos - output;
}

static void parse_minimal(const char *query, uint32_t query_len,
			  od_linear_alloc_t *arena, od_query_ctx_t *ctx)
{
	od_sql_minimal_node_t *ast =
		od_sql_minimal_parse(query, query_len, arena, NULL, NULL);

	od_sql_minimal_extract_query_ctx(ast, ctx);
}

typedef struct {
	char *errbuf;
	size_t size;
} parse_error_cb_arg_t;

static void full_on_error(const char *msg, void *userdata)
{
	parse_error_cb_arg_t *a = userdata;
	strncpy(a->errbuf, msg, a->size - 1);
	a->errbuf[a->size - 1] = '\0';
}

static void set_ctx_value(od_query_ctx_t *ctx, const char *value)
{
	value = value != NULL ? value : "";
	od_snprintf(ctx->s2, sizeof(ctx->s2), "%s", value);
	size_t len = strlen(value);
	if (len >= sizeof(ctx->s2)) {
		ctx->s2_long = od_malloc(len + 1);
		if (ctx->s2_long != NULL) {
			memcpy(ctx->s2_long, value, len + 1);
		}
	}
}

static void parse_full(const char *query, uint32_t query_len,
		       od_linear_alloc_t *arena, od_query_ctx_t *ctx)
{
	char errbuf[256];
	parse_error_cb_arg_t ea = { .errbuf = errbuf, .size = sizeof(errbuf) };
	List *tree =
		od_sql_full_parse(arena, query, query_len, full_on_error, &ea);

	ctx->flags = 0;
	ctx->s1[0] = '\0';
	ctx->s2[0] = '\0';
	ctx->s2_long = NULL;

	if (tree == NULL) {
		goto error;
	}

	if (list_length(tree) > 1) {
		/* do not support multi statements now */
		goto error;
	}

	RawStmt *rs = linitial(tree);
	if (rs == NULL || !IsA(rs, RawStmt)) {
		goto error;
	}

	Node *node = rs->stmt;
	if (node == NULL) {
		goto error;
	}

	if (IsA(node, VariableShowStmt)) {
		VariableShowStmt *vsstmt = castNode(VariableShowStmt, node);
		od_snprintf(ctx->s1, sizeof(ctx->s1), "%s", vsstmt->name);
		od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SHOW);
	} else if (IsA(node, VariableSetStmt)) {
		VariableSetStmt *vsstmt = castNode(VariableSetStmt, node);
		if (vsstmt->kind == VAR_SET_VALUE && vsstmt->args != NULL &&
		    list_length(vsstmt->args) == 1) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SET);
			od_snprintf(ctx->s1, sizeof(ctx->s1), "%s",
				    vsstmt->name);

			Node *arg = linitial(vsstmt->args);
			if (!IsA(arg, A_Const)) {
				goto error;
			}

			A_Const *acon = castNode(A_Const, arg);
			if (acon->isnull) {
				set_ctx_value(ctx, "");
			} else if (IsA(&acon->val, String)) {
				set_ctx_value(ctx, acon->val.sval.sval);
			} else if (IsA(&acon->val, Integer)) {
				char tmp[24];
				od_snprintf(tmp, sizeof(tmp), "%d",
					    acon->val.ival.ival);
				set_ctx_value(ctx, tmp);
			} else if (IsA(&acon->val, Float)) {
				set_ctx_value(ctx, acon->val.fval.fval);
			} else {
				goto error;
			}
		}
	} else if (IsA(node, TransactionStmt)) {
		TransactionStmt *tstmt = castNode(TransactionStmt, node);
		if (tstmt->kind == TRANS_STMT_BEGIN) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_BEGIN);
		}
	} else if (IsA(node, UnlistenStmt)) {
		UnlistenStmt *ustmt = castNode(UnlistenStmt, node);
		if (ustmt->conditionname == NULL) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_UNLISTEN_ALL);
		}
	} else if (IsA(node, DiscardStmt)) {
		DiscardStmt *dstmt = castNode(DiscardStmt, node);
		if (dstmt->target == DISCARD_ALL) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_DISCARD_ALL);
		}
	} else if (IsA(node, DeallocateStmt)) {
		DeallocateStmt *dstmt = castNode(DeallocateStmt, node);
		if (dstmt->isall) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_DEALLOCATE_ALL);
		} else if (dstmt->name != NULL) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_HAS_DEALLOCATE_NAME);
			od_snprintf(ctx->s1, sizeof(ctx->s1), "%s",
				    dstmt->name);
		}

	} else if (IsA(node, SelectStmt)) {
		/* TODO: will be done in next patches */
	}

	return;

error:
	od_query_ctx_set(ctx, OD_QUERY_CTX_PARSE_ERROR);
}

void od_query_parse_fill_ctx(const char *query, uint32_t query_len,
			     od_linear_alloc_t *arena, od_query_ctx_t *ctx,
			     const od_config_query_parsing_t *parsing)
{
	od_query_ctx_reset(ctx);

	if (parsing->mode == OD_CONFIG_QUERY_PARSING_MODE_DISABLED) {
		return;
	}

	if (query_len > parsing->max_query_len) {
		od_query_ctx_set(ctx, OD_QUERY_CTX_PARSE_ERROR);
		od_query_ctx_set(ctx, OD_QUERY_CTX_TOO_LONG);
		return;
	}

	if (parsing->mode == OD_CONFIG_QUERY_PARSING_MODE_MINIMAL) {
		parse_minimal(query, query_len, arena, ctx);
	} else if (parsing->mode == OD_CONFIG_QUERY_PARSING_MODE_FULL) {
		parse_full(query, query_len, arena, ctx);
	} else {
		od_release_assert(0);
	}
}

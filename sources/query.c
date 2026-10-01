
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

static int is_select_copy(CopyStmt *cstmt)
{
	if (cstmt->is_from) {
		return 0;
	}

	if (cstmt->filename != NULL) {
		return 0;
	}

	if (cstmt->query == NULL) {
		return 1;
	}

	return IsA(cstmt->query, SelectStmt);
}

static const char *safe_function_names[] = {
	"abs",
	"age",
	"array_agg",
	"array_cat",
	"array_dims",
	"array_length",
	"array_lower",
	"array_ndims",
	"array_position",
	"array_positions",
	"array_to_string",
	"array_upper",
	"ascii",
	"avg",
	"bit_and",
	"bit_or",
	"bool_and",
	"bool_or",
	"bool_xor",
	"btrim",
	"cardinality",
	"ceil",
	"ceiling",
	"char_length",
	"character_length",
	"chr",
	"concat",
	"concat_ws",
	"corr",
	"count",
	"covar_pop",
	"covar_samp",
	"current_database",
	"current_schema",
	"current_schemas",
	"cume_dist",
	"date_part",
	"date_trunc",
	"decode",
	"dense_rank",
	"div",
	"encode",
	"ends_with",
	"every",
	"exp",
	"extract",
	"first_value",
	"floor",
	"format",
	"gcd",
	"gen_random_uuid",
	"generate_series",
	"initcap",
	"isfinite",
	"json_agg",
	"json_build_array",
	"json_build_object",
	"json_extract_path",
	"json_extract_path_text",
	"json_object_agg",
	"jsonb_agg",
	"jsonb_build_array",
	"jsonb_build_object",
	"jsonb_extract_path",
	"jsonb_extract_path_text",
	"jsonb_object_agg",
	"jsonb_pretty",
	"jsonb_set",
	"jsonb_typeof",
	"json_typeof",
	"lag",
	"last_value",
	"lcm",
	"lead",
	"left",
	"length",
	"ln",
	"log",
	"lower",
	"lpad",
	"ltrim",
	"make_date",
	"make_interval",
	"make_time",
	"make_timestamp",
	"make_timestamptz",
	"max",
	"md5",
	"min",
	"mode",
	"mod",
	"now",
	"nth_value",
	"ntile",
	"overlay",
	"percent_rank",
	"percentile_cont",
	"percentile_disc",
	"pi",
	"position",
	"pow",
	"power",
	"pg_backend_pid",
	"pg_column_size",
	"pg_database_size",
	"pg_get_expr",
	"pg_indexes_size",
	"pg_is_in_recovery",
	"pg_relation_size",
	"pg_size_pretty",
	"pg_table_size",
	"pg_total_relation_size",
	"quote_ident",
	"quote_literal",
	"quote_nullable",
	"random",
	"rank",
	"regexp_match",
	"regexp_matches",
	"regexp_replace",
	"regexp_split_to_array",
	"regexp_split_to_table",
	"regexp_substr",
	"repeat",
	"replace",
	"reverse",
	"right",
	"round",
	"row_number",
	"row_to_json",
	"rpad",
	"rtrim",
	"scale",
	"sign",
	"split_part",
	"sqrt",
	"starts_with",
	"strpos",
	"string_agg",
	"string_to_array",
	"substr",
	"substring",
	"sum",
	"to_char",
	"to_date",
	"to_hex",
	"to_json",
	"to_jsonb",
	"to_number",
	"to_timestamp",
	"translate",
	"trim",
	"trim_scale",
	"trunc",
	"txid_current",
	"unistr",
	"unnest",
	"upper",
	"version",
	"width_bucket",
	"xmlagg",
};

static int is_safe_function_name(const char *name)
{
	size_t i;

	for (i = 0; i < lengthof(safe_function_names); i++) {
		if (strcmp(safe_function_names[i], name) == 0) {
			return 1;
		}
	}

	return 0;
}

static int is_safe_function_call(FuncCall *fcall)
{
	List *names = fcall->funcname;

	if (names == NULL || list_length(names) == 0 ||
	    list_length(names) > 2) {
		return 0;
	}

	if (list_length(names) == 2 &&
	    strcmp(strVal(linitial(names)), "pg_catalog") != 0) {
		return 0;
	}

	return is_safe_function_name(strVal(llast(names)));
}

static int has_function_call_walker(Node *node)
{
	if (node == NULL) {
		return 0;
	}

#define WALK(n) has_function_call_walker((Node *)(n))

	switch (nodeTag(node)) {
	/* primitive node types with no subnodes */
	case T_JsonFormat:
	case T_SetToDefault:
	case T_CurrentOfExpr:
	case T_SQLValueFunction:
	case T_Integer:
	case T_Float:
	case T_Boolean:
	case T_String:
	case T_BitString:
	case T_ParamRef:
	case T_A_Const:
	case T_A_Star:
	case T_MergeSupportFunc:
	case T_ReturningOption:
	case T_Alias:
	case T_ColumnRef:
	case T_JsonTablePathSpec:
		break;
	case T_RangeVar:
		return WALK(((RangeVar *)node)->alias);
	case T_GroupingFunc:
		return WALK(((GroupingFunc *)node)->args);
	case T_SubLink:
		return WALK(((SubLink *)node)->testexpr) ||
		       WALK(((SubLink *)node)->subselect);
	case T_CaseExpr: {
		CaseExpr *caseexpr = (CaseExpr *)node;
		ListCell *temp;

		if (WALK(caseexpr->arg)) {
			return 1;
		}
		foreach(temp, caseexpr->args)
		{
			CaseWhen *when = lfirst_node(CaseWhen, temp);

			if (WALK(when->expr) || WALK(when->result)) {
				return 1;
			}
		}
		return WALK(caseexpr->defresult);
	}
	case T_RowExpr:
		return WALK(((RowExpr *)node)->args);
	case T_CoalesceExpr:
		return WALK(((CoalesceExpr *)node)->args);
	case T_MinMaxExpr:
		return WALK(((MinMaxExpr *)node)->args);
	case T_XmlExpr: {
		XmlExpr *xexpr = (XmlExpr *)node;

		return WALK(xexpr->named_args) || WALK(xexpr->args);
	}
	case T_JsonReturning:
		return WALK(((JsonReturning *)node)->format);
	case T_JsonValueExpr: {
		JsonValueExpr *jve = (JsonValueExpr *)node;

		return WALK(jve->raw_expr) || WALK(jve->formatted_expr) ||
		       WALK(jve->format);
	}
	case T_JsonParseExpr: {
		JsonParseExpr *jpe = (JsonParseExpr *)node;

		return WALK(jpe->expr) || WALK(jpe->output);
	}
	case T_JsonScalarExpr: {
		JsonScalarExpr *jse = (JsonScalarExpr *)node;

		return WALK(jse->expr) || WALK(jse->output);
	}
	case T_JsonSerializeExpr: {
		JsonSerializeExpr *jse = (JsonSerializeExpr *)node;

		return WALK(jse->expr) || WALK(jse->output);
	}
	case T_JsonArgument:
		return WALK(((JsonArgument *)node)->val);
	case T_JsonFuncExpr: {
		JsonFuncExpr *jfe = (JsonFuncExpr *)node;

		return WALK(jfe->context_item) || WALK(jfe->pathspec) ||
		       WALK(jfe->passing) || WALK(jfe->output) ||
		       WALK(jfe->on_empty) || WALK(jfe->on_error);
	}
	case T_JsonBehavior:
		return WALK(((JsonBehavior *)node)->expr);
	case T_JsonTable: {
		JsonTable *jt = (JsonTable *)node;

		return WALK(jt->context_item) || WALK(jt->passing) ||
		       WALK(jt->columns) || WALK(jt->on_error);
	}
	case T_JsonTableColumn: {
		JsonTableColumn *jtc = (JsonTableColumn *)node;

		return WALK(jtc->typeName) || WALK(jtc->on_empty) ||
		       WALK(jtc->on_error) || WALK(jtc->columns);
	}
	case T_NullTest:
		return WALK(((NullTest *)node)->arg);
	case T_BooleanTest:
		return WALK(((BooleanTest *)node)->arg);
	case T_JoinExpr: {
		JoinExpr *join = (JoinExpr *)node;

		return WALK(join->larg) || WALK(join->rarg) ||
		       WALK(join->quals) || WALK(join->alias);
	}
	case T_IntoClause: {
		IntoClause *into = (IntoClause *)node;

		return WALK(into->rel) || WALK(into->viewQuery);
	}
	case T_List: {
		ListCell *temp;

		foreach(temp, (List *)node)
		{
			if (WALK(lfirst(temp))) {
				return 1;
			}
		}
		return 0;
	}
	case T_InsertStmt: {
		InsertStmt *stmt = (InsertStmt *)node;

		return WALK(stmt->relation) || WALK(stmt->cols) ||
		       WALK(stmt->selectStmt) || WALK(stmt->onConflictClause) ||
		       WALK(stmt->returningClause) || WALK(stmt->withClause);
	}
	case T_DeleteStmt: {
		DeleteStmt *stmt = (DeleteStmt *)node;

		return WALK(stmt->relation) || WALK(stmt->usingClause) ||
		       WALK(stmt->whereClause) || WALK(stmt->returningClause) ||
		       WALK(stmt->withClause);
	}
	case T_UpdateStmt: {
		UpdateStmt *stmt = (UpdateStmt *)node;

		return WALK(stmt->relation) || WALK(stmt->targetList) ||
		       WALK(stmt->whereClause) || WALK(stmt->fromClause) ||
		       WALK(stmt->returningClause) || WALK(stmt->withClause);
	}
	case T_MergeStmt: {
		MergeStmt *stmt = (MergeStmt *)node;

		return WALK(stmt->relation) || WALK(stmt->sourceRelation) ||
		       WALK(stmt->joinCondition) ||
		       WALK(stmt->mergeWhenClauses) ||
		       WALK(stmt->returningClause) || WALK(stmt->withClause);
	}
	case T_MergeWhenClause: {
		MergeWhenClause *clause = (MergeWhenClause *)node;

		return WALK(clause->condition) || WALK(clause->targetList) ||
		       WALK(clause->values);
	}
	case T_ReturningClause: {
		ReturningClause *returning = (ReturningClause *)node;

		return WALK(returning->options) || WALK(returning->exprs);
	}
	case T_SelectStmt: {
		SelectStmt *stmt = (SelectStmt *)node;

		return WALK(stmt->distinctClause) || WALK(stmt->intoClause) ||
		       WALK(stmt->targetList) || WALK(stmt->fromClause) ||
		       WALK(stmt->whereClause) || WALK(stmt->groupClause) ||
		       WALK(stmt->havingClause) || WALK(stmt->windowClause) ||
		       WALK(stmt->valuesLists) || WALK(stmt->sortClause) ||
		       WALK(stmt->limitOffset) || WALK(stmt->limitCount) ||
		       WALK(stmt->lockingClause) || WALK(stmt->withClause) ||
		       WALK(stmt->larg) || WALK(stmt->rarg);
	}
	case T_PLAssignStmt: {
		PLAssignStmt *stmt = (PLAssignStmt *)node;

		return WALK(stmt->indirection) || WALK(stmt->val);
	}
	case T_A_Expr: {
		A_Expr *expr = (A_Expr *)node;

		return WALK(expr->lexpr) || WALK(expr->rexpr);
	}
	case T_BoolExpr:
		return WALK(((BoolExpr *)node)->args);
	case T_FuncCall: {
		FuncCall *fcall = (FuncCall *)node;

		/* known safe functions are descended into, not reported */
		if (is_safe_function_call(fcall)) {
			return WALK(fcall->args) || WALK(fcall->agg_order) ||
			       WALK(fcall->agg_filter) || WALK(fcall->over);
		}

		/* a function call has been found */
		return 1;
	}
	case T_NamedArgExpr:
		return WALK(((NamedArgExpr *)node)->arg);
	case T_A_Indices: {
		A_Indices *indices = (A_Indices *)node;

		return WALK(indices->lidx) || WALK(indices->uidx);
	}
	case T_A_Indirection: {
		A_Indirection *indir = (A_Indirection *)node;

		return WALK(indir->arg) || WALK(indir->indirection);
	}
	case T_A_ArrayExpr:
		return WALK(((A_ArrayExpr *)node)->elements);
	case T_ResTarget: {
		ResTarget *rt = (ResTarget *)node;

		return WALK(rt->indirection) || WALK(rt->val);
	}
	case T_MultiAssignRef:
		return WALK(((MultiAssignRef *)node)->source);
	case T_TypeCast: {
		TypeCast *tc = (TypeCast *)node;

		return WALK(tc->arg) || WALK(tc->typeName);
	}
	case T_CollateClause:
		return WALK(((CollateClause *)node)->arg);
	case T_SortBy:
		return WALK(((SortBy *)node)->node);
	case T_WindowDef: {
		WindowDef *wd = (WindowDef *)node;

		return WALK(wd->partitionClause) || WALK(wd->orderClause) ||
		       WALK(wd->startOffset) || WALK(wd->endOffset);
	}
	case T_RangeSubselect: {
		RangeSubselect *rs = (RangeSubselect *)node;

		return WALK(rs->subquery) || WALK(rs->alias);
	}
	case T_RangeFunction: {
		RangeFunction *rf = (RangeFunction *)node;

		return WALK(rf->functions) || WALK(rf->alias) ||
		       WALK(rf->coldeflist);
	}
	case T_RangeTableSample: {
		RangeTableSample *rts = (RangeTableSample *)node;

		return WALK(rts->relation) || WALK(rts->args) ||
		       WALK(rts->repeatable);
	}
	case T_RangeTableFunc: {
		RangeTableFunc *rtf = (RangeTableFunc *)node;

		return WALK(rtf->docexpr) || WALK(rtf->rowexpr) ||
		       WALK(rtf->namespaces) || WALK(rtf->columns) ||
		       WALK(rtf->alias);
	}
	case T_RangeTableFuncCol: {
		RangeTableFuncCol *rtfc = (RangeTableFuncCol *)node;

		return WALK(rtfc->colexpr) || WALK(rtfc->coldefexpr);
	}
	case T_TypeName: {
		TypeName *tn = (TypeName *)node;

		return WALK(tn->typmods) || WALK(tn->arrayBounds);
	}
	case T_ColumnDef: {
		ColumnDef *coldef = (ColumnDef *)node;

		return WALK(coldef->typeName) || WALK(coldef->raw_default) ||
		       WALK(coldef->collClause);
	}
	case T_IndexElem:
		return WALK(((IndexElem *)node)->expr);
	case T_GroupingSet:
		return WALK(((GroupingSet *)node)->content);
	case T_LockingClause:
		return WALK(((LockingClause *)node)->lockedRels);
	case T_XmlSerialize: {
		XmlSerialize *xs = (XmlSerialize *)node;

		return WALK(xs->expr) || WALK(xs->typeName);
	}
	case T_WithClause:
		return WALK(((WithClause *)node)->ctes);
	case T_InferClause: {
		InferClause *stmt = (InferClause *)node;

		return WALK(stmt->indexElems) || WALK(stmt->whereClause);
	}
	case T_OnConflictClause: {
		OnConflictClause *stmt = (OnConflictClause *)node;

		return WALK(stmt->infer) || WALK(stmt->targetList) ||
		       WALK(stmt->whereClause);
	}
	case T_CommonTableExpr:
		return WALK(((CommonTableExpr *)node)->ctequery);
	case T_JsonOutput: {
		JsonOutput *out = (JsonOutput *)node;

		return WALK(out->typeName) || WALK(out->returning);
	}
	case T_JsonObjectConstructor: {
		JsonObjectConstructor *joc = (JsonObjectConstructor *)node;

		return WALK(joc->output) || WALK(joc->exprs);
	}
	case T_JsonArrayConstructor: {
		JsonArrayConstructor *jac = (JsonArrayConstructor *)node;

		return WALK(jac->output) || WALK(jac->exprs);
	}
	case T_JsonAggConstructor: {
		JsonAggConstructor *ctor = (JsonAggConstructor *)node;

		return WALK(ctor->output) || WALK(ctor->agg_order) ||
		       WALK(ctor->agg_filter) || WALK(ctor->over);
	}
	case T_JsonObjectAgg: {
		JsonObjectAgg *joa = (JsonObjectAgg *)node;

		return WALK(joa->constructor) || WALK(joa->arg);
	}
	case T_JsonArrayAgg: {
		JsonArrayAgg *jaa = (JsonArrayAgg *)node;

		return WALK(jaa->constructor) || WALK(jaa->arg);
	}
	case T_JsonArrayQueryConstructor: {
		JsonArrayQueryConstructor *jaqc =
			(JsonArrayQueryConstructor *)node;

		return WALK(jaqc->output) || WALK(jaqc->query);
	}
	default:
		break;
	}

#undef WALK
	return 0;
}

static int has_function_calls(Node *node)
{
	if (node == NULL || !IsA(node, SelectStmt)) {
		return 0;
	}

	return has_function_call_walker(node);
}

static int is_select_explain(ExplainStmt *estmt)
{
	Node *query = estmt->query;
	ListCell *lc;
	int analyze = 0;

	foreach(lc, estmt->options)
	{
		DefElem *opt = lfirst_node(DefElem, lc);

		if (strcmp(opt->defname, "analyze") == 0) {
			analyze = 1;
			break;
		}
	}

	if (IsA(query, SelectStmt)) {
		if (!analyze) {
			return 1;
		}

		return !has_function_calls(query);
	}

	return 0;
}

static int is_select_select(SelectStmt *sstmt)
{
	if (sstmt->intoClause != NULL || sstmt->lockingClause != NULL) {
		return 0;
	}

	if (sstmt->withClause != NULL) {
		ListCell *cte_item;

		foreach(cte_item, sstmt->withClause->ctes)
		{
			CommonTableExpr *cte =
				lfirst_node(CommonTableExpr, cte_item);

			if (!IsA(cte->ctequery, SelectStmt)) {
				return 0;
			}
		}
	}

	return !has_function_calls((Node *)sstmt);
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
		od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SELECT);
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
		/* inspired by https://github.com/pgpool/pgpool2/blob/91c8522/src/protocol/pool_process_query.c#L1140 */
		SelectStmt *sstmt = castNode(SelectStmt, node);
		if (is_select_select(sstmt)) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SELECT);
		}
	} else if (IsA(node, CopyStmt)) {
		CopyStmt *cstmt = castNode(CopyStmt, node);
		if (is_select_copy(cstmt)) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SELECT);
		}
	} else if (IsA(node, ExplainStmt)) {
		ExplainStmt *estmt = castNode(ExplainStmt, node);
		if (is_select_explain(estmt)) {
			od_query_ctx_set(ctx, OD_QUERY_CTX_IS_SELECT);
		}
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

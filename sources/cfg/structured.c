/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <libfyaml.h>
#include <cfg/keywords.h>
#include <cfg/parse.tab.h>
#include <cfg/structured.h>

struct od_cfg_structured {
	od_cfg_parse_ctx_t *ctx;
	struct fy_diag *diag;
	yypstate *state;
	int status;
	od_cfg_location_t location;
};

static od_cfg_location_t token_location(struct fy_token *token,
					od_cfg_location_t loc)
{
	const struct fy_mark *start = fy_token_start_mark(token);
	const struct fy_mark *end = fy_token_end_mark(token);
	if (start != NULL && end != NULL) {
		loc.first_line = start->line + 1;
		loc.first_column = start->column + 1;
		loc.first_offset = start->input_pos;
		loc.last_line = end->line + 1;
		loc.last_column = end->column + 1;
		loc.last_offset = end->input_pos;
	}
	return loc;
}

int od_cfg_structured_error(od_cfg_structured_t *s, struct fy_node *node,
			    const char *message)
{
	if (node != NULL) {
		/* libfyaml can also locate collection nodes and their anchors. */
		fy_diag_node_report(s->diag, node, FYET_ERROR, "%s", message);
	} else {
		od_cfg_diag_error(s->ctx->diags, s->location, "%s", message);
	}
	return -1;
}

int od_cfg_check_node(od_cfg_structured_t *s, struct fy_node *node)
{
	if (node == NULL || fy_node_is_null(node)) {
		return od_cfg_structured_error(s, node,
					       "expected a non-null value");
	}
	if (fy_node_get_tag_token(node) != NULL || fy_node_is_alias(node)) {
		return od_cfg_structured_error(
			s, node, "YAML tags and aliases are unsupported");
	}
	if (fy_node_is_scalar(node)) {
		size_t size;
		const char *text = fy_node_get_scalar(node, &size);
		if (text == NULL || memchr(text, 0, size) != NULL) {
			return od_cfg_structured_error(
				s, node, "invalid scalar or embedded NUL");
		}
		if (fy_node_get_style(node) == FYNS_PLAIN &&
		    ((size == 1 && text[0] == '~') ||
		     (size == 4 && (memcmp(text, "null", 4) == 0 ||
				    memcmp(text, "Null", 4) == 0 ||
				    memcmp(text, "NULL", 4) == 0)))) {
			return od_cfg_structured_error(
				s, node, "expected a non-null value");
		}
	}
	return 0;
}

static int push(od_cfg_structured_t *s, struct fy_node *node, int token,
		YYSTYPE value)
{
	if (node != NULL) {
		s->location = token_location(fy_node_get_scalar_token(node),
					     s->location);
	}
	s->status = yypush_parse(s->state, token, &value, &s->location, NULL,
				 s->ctx);
	return s->status == YYPUSH_MORE || s->status == 0 ? 0 : -1;
}

int od_cfg_emit(od_cfg_structured_t *s, struct fy_node *node, int token)
{
	return push(s, node, token, (YYSTYPE){ 0 });
}

static int push_string(od_cfg_structured_t *s, struct fy_node *node,
		       const char *text)
{
	char *copy = od_strdup(text);
	if (copy == NULL) {
		return od_cfg_structured_error(s, node, "out of memory");
	}
	return push(s, node, STRING, (YYSTYPE){ .str = copy });
}

/* libfyaml returns scalar text and style, including for JSON numbers and
 * booleans. Keep types distinct so a password such as "001" stays intact. */
static int scalar_type(struct fy_node *node, const char *text)
{
	if (fy_node_get_style(node) != FYNS_PLAIN) {
		return STRING;
	}
	if (strcmp(text, "true") == 0 || strcmp(text, "True") == 0 ||
	    strcmp(text, "TRUE") == 0) {
		return YES;
	}
	if (strcmp(text, "false") == 0 || strcmp(text, "False") == 0 ||
	    strcmp(text, "FALSE") == 0) {
		return NO;
	}
	const char *d = text + (*text == '+' || *text == '-');
	if (!isdigit((unsigned char)*d) && *d != '.') {
		return STRING;
	}
	char *end;
	strtod(text, &end);
	if (end != text && *end == 0) {
		return NUMBER;
	}
	return STRING;
}

static const char *scalar_text(od_cfg_structured_t *s, struct fy_node *node)
{
	if (od_cfg_check_node(s, node) != 0) {
		return NULL;
	}
	if (!fy_node_is_scalar(node)) {
		od_cfg_structured_error(s, node, "expected a scalar value");
		return NULL;
	}
	const char *text = fy_node_get_scalar0(node);
	if (text == NULL) {
		od_cfg_structured_error(s, node, "out of memory");
	}
	return text;
}

int od_cfg_emit_default(od_cfg_structured_t *s, struct fy_node *node)
{
	const char *text = scalar_text(s, node);
	if (text == NULL) {
		return -1;
	}
	if (scalar_type(node, text) != YES) {
		return od_cfg_structured_error(s, node,
					       "expected default: true");
	}
	return od_cfg_emit(s, node, DEFAULT);
}

int od_cfg_emit_string(od_cfg_structured_t *s, struct fy_node *node)
{
	const char *text = scalar_text(s, node);
	if (text == NULL) {
		return -1;
	}
	if (scalar_type(node, text) != STRING) {
		return od_cfg_structured_error(s, node, "expected a string");
	}
	return push_string(s, node, text);
}

int od_cfg_emit_value(od_cfg_structured_t *s, int directive,
		      struct fy_node *node)
{
	const char *text = scalar_text(s, node);
	if (text == NULL) {
		return -1;
	}
	if (directive == QUERY_PARSING_MODE) {
		int token = od_cfg_keyword_lookup(text, strlen(text));
		if (token != MINIMAL && token != FULL && token != DISABLED) {
			return od_cfg_structured_error(
				s, node, "expected minimal, full or disabled");
		}
		return od_cfg_emit(s, node, token);
	}
	int type = scalar_type(node, text);
	if (type == YES || type == NO) {
		return od_cfg_emit(s, node, type);
	}
	if (type == NUMBER) {
		char *end;
		int64_t value = od_cfg_read_integer(text, &end);
		if (*end == 0) {
			if (errno == ERANGE) {
				return od_cfg_structured_error(
					s, node, "integer is out of range");
			}
			return push(s, node, NUMBER, (YYSTYPE){ .num = value });
		}
		errno = 0;
		double real = strtod(text, &end);
		if (errno == ERANGE || !isfinite(real)) {
			return od_cfg_structured_error(
				s, node, "number is out of range");
		}
		return push(s, node, REAL, (YYSTYPE){ .real = real });
	}
	/* These grammar productions take size_value; a suffix makes the
	 * scalar a string in YAML and JSON. */
	switch (directive) {
	case SOFT_OOM_LIMIT:
	case READAHEAD:
	case WRITEBUF:
	case QUERY_PARSING_MEM_LIMIT: {
		int64_t value;
		const char *error = od_cfg_parse_size(text, &value);
		if (error != NULL) {
			return od_cfg_structured_error(s, node, error);
		}
		return push(s, node, SIZE_NUMBER, (YYSTYPE){ .num = value });
	}
	default:
		break;
	}
	/* time_value and bool_value accept STRING and perform their own
	 * conversion and validation, just as they do for native configs. */
	return push_string(s, node, text);
}

static void report_diagnostics(od_cfg_structured_t *s)
{
	void *iter = NULL;
	struct fy_diag_error *error;
	while ((error = fy_diag_errors_iterate(s->diag, &iter)) != NULL) {
		if (error->type != FYET_ERROR) {
			continue;
		}
		od_cfg_location_t loc = od_cfg_location_empty(s->ctx->filename);
		loc.first_line = loc.last_line = error->line;
		loc.first_column = loc.last_column = error->column;
		od_cfg_diag_error(s->ctx->diags,
				  token_location(error->fyt, loc), "%s",
				  error->msg);
	}
}

int od_cfg_parse_structured(od_cfg_parse_ctx_t *ctx, bool json)
{
	od_cfg_structured_t s = {
		.ctx = ctx,
		.status = YYPUSH_MORE,
		.location = od_cfg_location_empty(ctx->filename),
	};
	const char *nul = memchr(ctx->input, 0, ctx->input_size);
	if (nul != NULL) {
		for (const char *p = ctx->input; p < nul; p++) {
			if (*p == '\n') {
				s.location.first_line++;
				s.location.first_column = 1;
			} else {
				s.location.first_column++;
			}
		}
		s.location.last_line = s.location.first_line;
		s.location.last_column = s.location.first_column + 1;
		s.location.first_offset = (size_t)(nul - ctx->input);
		s.location.last_offset = s.location.first_offset + 1;
		return od_cfg_structured_error(&s, NULL,
					       "embedded NUL in input");
	}
	struct fy_parse_cfg cfg = {
		.flags = FYPCF_QUIET | FYPCF_COLLECT_DIAG |
			 FYPCF_DEFAULT_VERSION_1_2 |
			 (json ? FYPCF_JSON_FORCE : FYPCF_JSON_NONE),
	};
	struct fy_parser *parser = fy_parser_create(&cfg);
	if (parser == NULL) {
		return od_cfg_structured_error(
			&s, NULL, "failed to initialize YAML/JSON parser");
	}
	s.diag = fy_parser_get_diag(parser);
	fy_diag_set_collect_errors(s.diag, true);

	int rc = -1;
	struct fy_document *doc = NULL;
	struct fy_document *extra = NULL;
	if (fy_parser_set_string(parser, ctx->input, ctx->input_size) < 0) {
		od_cfg_structured_error(&s, NULL,
					"failed to read YAML/JSON input");
		goto done;
	}
	doc = fy_parse_load_document(parser);
	if (doc == NULL) {
		if (!fy_diag_got_error(s.diag)) {
			od_cfg_structured_error(
				&s, NULL, "expected a configuration document");
		}
		goto done;
	}
	extra = fy_parse_load_document(parser);
	if (extra != NULL) {
		od_cfg_structured_error(
			&s, fy_document_root(extra),
			"expected a single configuration document");
		goto done;
	}
	if (fy_parser_get_stream_error(parser) || fy_diag_got_error(s.diag)) {
		goto done;
	}
	s.state = yypstate_new();
	if (s.state == NULL) {
		od_cfg_structured_error(&s, NULL, "out of memory");
		goto done;
	}
	rc = od_cfg_structured_map(&s, fy_document_root(doc));
	if (s.status == YYPUSH_MORE) {
		/* A scanner-reported error unwinds the semantic stack without
		 * adding a spurious "unexpected EOF" diagnostic. */
		if (rc != 0) {
			od_cfg_emit(&s, NULL, YYerror);
		}
		if (s.status == YYPUSH_MORE) {
			od_cfg_emit(&s, NULL, YYEOF);
		}
	}
	if (s.status != 0) {
		rc = -1;
	}
	yypstate_delete(s.state);
done:
	report_diagnostics(&s);
	/* Release collected errors and their references to input tokens. */
	fy_diag_set_collect_errors(s.diag, false);
	if (extra != NULL) {
		fy_parse_document_destroy(parser, extra);
	}
	if (doc != NULL) {
		fy_parse_document_destroy(parser, doc);
	}
	fy_diag_unref(s.diag);
	fy_parser_destroy(parser);
	return rc;
}

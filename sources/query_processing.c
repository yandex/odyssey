/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <parser.h>
#include <query_processing.h>

od_keyword_t od_query_process_keywords[] = {
	od_keyword("odyssey", OD_QUERY_PROCESSING_LODYSSEY),
	od_keyword("target_session_attrs", OD_QUERY_PROCESSING_LTSA),
	od_keyword("application_name", OD_QUERY_PROCESSING_LAPPNAME),
	od_keyword("set", OD_QUERY_PROCESSING_LSET),
	od_keyword("to", OD_QUERY_PROCESSING_LTO),
	od_keyword("show", OD_QUERY_PROCESSING_LSHOW),
	od_keyword("deallocate", OD_QUERY_PROCESSING_DEALLOCATE),
	od_keyword("prepare", OD_QUERY_PROCESSING_PREPARE),
	od_keyword("all", OD_QUERY_PROCESSING_ALL),
	{ 0, 0, 0 }
};

static int parse_deallocate_name(od_parser_t *parser, const od_token_t *nametok,
				 const char **name, size_t *name_len)
{
	od_token_t token;
	int rc = od_parser_next(parser, &token);
	int eof = rc == OD_PARSER_EOF;
	int semicolon = rc == OD_PARSER_SYMBOL && token.value.num == ';';
	if (!eof && !semicolon) {
		/* some other bytes after DEALLOCATE [PREPARE] name <here> */
		return -1;
	}

	size_t tlen = (size_t)nametok->value.string.size;
	const char *tok = nametok->value.string.pointer;

	if (nametok->type == OD_PARSER_KEYWORD && tlen == 3 &&
	    strncasecmp(tok, "ALL", 3) == 0) {
		return 1;
	}

	*name = tok;
	*name_len = tlen;

	return 0;
}

static int parse_deallocate_impl(od_parser_t *parser, const char **name,
				 size_t *name_len)
{
	int rc;
	od_token_t token;
	od_keyword_t *keyword;

	/*
	 * [PREPARE] name | ALL
	 */
	rc = od_parser_next(parser, &token);
	switch (rc) {
	case OD_PARSER_KEYWORD:
		keyword = od_keyword_match(od_query_process_keywords, &token);

		/* DEALLOCATE pstmt */
		if (keyword == NULL) {
			return parse_deallocate_name(parser, &token, name,
						     name_len);
		}

		if (keyword->id == OD_QUERY_PROCESSING_ALL) {
			/* DEALLOCATE ALL */
			return parse_deallocate_name(parser, &token, name,
						     name_len);
		} else if (keyword->id == OD_QUERY_PROCESSING_PREPARE) {
			/* skip PREPARE, run again */
			return parse_deallocate_impl(parser, name, name_len);
		}
		break;
	case OD_PARSER_STRING:
		/* DEALLOCATE 'pstmt' */
		return parse_deallocate_name(parser, &token, name, name_len);
	default:
		return -1;
	}

	return -1;
}

int od_parse_deallocate_parser(od_parser_t *parser, const char **name,
			       size_t *name_len)
{
	return parse_deallocate_impl(parser, name, name_len);
}

int od_parse_deallocate(const char *query, size_t query_len, const char **name,
			size_t *name_len)
{
	*name = NULL;
	*name_len = 0;

	od_parser_t parser;
	od_parser_init_queries_mode(&parser, query, query_len);

	od_token_t token;
	int rc = od_parser_next(&parser, &token);

	if (rc != OD_PARSER_KEYWORD) {
		return -1;
	}

	od_keyword_t *keyword;
	keyword = od_keyword_match(od_query_process_keywords, &token);

	if (keyword == NULL) {
		return -1;
	}

	if (keyword->id == OD_QUERY_PROCESSING_DEALLOCATE) {
		return parse_deallocate_impl(&parser, name, name_len);
	}

	return -1;
}

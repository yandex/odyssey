#include <arpa/inet.h>

#include "odyssey.h"

enum {
	OD_LLOCAL,
	OD_LHOST,
	OD_LHOSTSSL,
	OD_LHOSTNOSSL,
	OD_LALL,
};

static od_keyword_t od_hba_keywords[] = {
	/* connection types */
	od_keyword("local", OD_LLOCAL),
	od_keyword("host", OD_LHOST),
	od_keyword("hostssl", OD_LHOSTSSL),
	od_keyword("hostnossl", OD_LHOSTNOSSL),
	/* db/user */
	od_keyword("all", OD_LALL),
};

static int
od_hba_parser_next(od_parser_t *parser, od_token_t *token)
{
    /* skip white spaces and comments */
    for (;;) {
        while (parser->pos < parser->end && isspace(*parser->pos)) {
            if (*parser->pos == '\n')
                parser->line++;
            parser->pos++;
        }
        if (od_unlikely(parser->pos == parser->end)) {
            token->type = OD_PARSER_EOF;
            return token->type;
        }
        if (*parser->pos != '#')
            break;
        while (parser->pos < parser->end && *parser->pos != '\n')
            parser->pos++;
        if (parser->pos == parser->end) {
            token->type = OD_PARSER_EOF;
            return token->type;
        }
        parser->line++;
    }

    /* symbols */
    if (*parser->pos != '\"' && ispunct(*parser->pos)) {
        token->type      = OD_PARSER_SYMBOL;
        token->line      = parser->line;
        token->value.num = *parser->pos;
        parser->pos++;
        return token->type;
    }

    if (isalnum(*parser->pos)) {
        token->type                 = OD_PARSER_KEYWORD;
        token->line                 = parser->line;
        token->value.string.pointer = parser->pos;
        while (parser->pos < parser->end && *parser->pos != ',' &&
               (isalnum(*parser->pos) || ispunct(*parser->pos)))
            parser->pos++;
        token->value.string.size = parser->pos - token->value.string.pointer;
        return token->type;
    }

    if (*parser->pos == '\"') {
        token->type = OD_PARSER_STRING;
        token->line = parser->line;
        parser->pos++;
        token->value.string.pointer = parser->pos;
        while (parser->pos < parser->end && *parser->pos != '\"') {
            if (*parser->pos == '\n') {
                token->type = OD_PARSER_ERROR;
                return token->type;
            }
            parser->pos++;
        }
        if (od_unlikely(parser->pos == parser->end)) {
            token->type = OD_PARSER_ERROR;
            return token->type;
        }
        token->value.string.size = parser->pos - token->value.string.pointer;
        parser->pos++;
        return token->type;
    }

    /* error */
    token->type = OD_PARSER_ERROR;
    token->line = parser->line;
    return token->type;
}

static bool
od_hba_reader_keyword(od_config_reader_t *reader, od_keyword_t *keyword)
{
    od_token_t token;
    int rc;
    rc = od_hba_parser_next(&reader->parser, &token);
    if (rc != OD_PARSER_KEYWORD)
        goto error;
    od_keyword_t *match;
    match = od_keyword_match(od_hba_keywords, &token);
    if (keyword == NULL)
        goto error;
    if (keyword != match)
        goto error;
    return true;
    error:
    od_parser_push(&reader->parser, &token);
    char *kwname = "unknown";
    if (keyword)
        kwname = keyword->name;
//    od_config_reader_error(reader, &token, "expected '%s'", kwname);
    return false;
}

static bool
od_hba_reader_string(od_config_reader_t *reader, char **value)
{
    od_token_t token;
    int rc;
    rc = od_hba_parser_next(&reader->parser, &token);
    if (rc != OD_PARSER_STRING) {
        od_parser_push(&reader->parser, &token);
//        od_config_reader_error(reader, &token, "expected 'string'");
        return false;
    }
    char *copy = malloc(token.value.string.size + 1);
    if (copy == NULL) {
        od_parser_push(&reader->parser, &token);
//        od_config_reader_error(reader, &token, "memory allocation error");
        return false;
    }
    memcpy(copy, token.value.string.pointer, token.value.string.size);
    copy[token.value.string.size] = 0;
    if (*value)
        free(*value);
    *value = copy;
    return true;
}

static bool
od_hba_reader_match_string(od_token_t token, char **value)
{
    char *copy = malloc(token.value.string.size + 1);
    if (copy == NULL) {
//        od_config_reader_error(reader, &token, "memory allocation error");
        return false;
    }
    memcpy(copy, token.value.string.pointer, token.value.string.size);
    copy[token.value.string.size] = 0;
    if (*value)
        free(*value);
    *value = copy;
    return true;
}

static int
od_hba_reader_value(od_config_reader_t *reader, void *dest)
{
    od_token_t token;
    int rc;
	char *string_value = NULL;
    rc = od_hba_parser_next(&reader->parser, &token);
	if (rc == OD_PARSER_KEYWORD) {
        od_keyword_t *match;
        match = od_keyword_match(od_hba_keywords, &token);
		if (match) {
			dest = match;
			return OD_PARSER_KEYWORD;
		}
		if (od_hba_reader_match_string(token, &string_value)) {
			dest = string_value;
			return OD_PARSER_STRING;
		}
	} else if (rc == OD_PARSER_STRING) {
        if (od_hba_reader_match_string(token, &string_value)) {
            dest = string_value;
            return OD_PARSER_STRING;
        }
	} else {
		return -1;
	}
}

static int
od_hba_reader_address(struct sockaddr_storage *dest, const char *addr)
{
    int rc;
    rc = inet_pton(AF_INET, addr, &((struct sockaddr_in *)dest)->sin_addr);
    if (rc > 0) {
        dest->ss_family = AF_INET;
        return 0;
    }
    if (inet_pton(AF_INET6, addr, &((struct sockaddr_in6 *)dest)->sin6_addr) > 0) {
        dest->ss_family = AF_INET6;
        return 0;
    }
    return -1;
}

static int
od_hba_reader_prefix(od_config_hba_t *hba, char *prefix)
{
    char *end = NULL;
    unsigned long int len = strtoul(prefix, &end, 10);

    if (hba->addr.ss_family == AF_INET) {
        if (len > 32)
            return -1;
        uint32_t mask = 0;
        unsigned int i;
        struct sockaddr_in *addr = (struct sockaddr_in *)&hba->mask;
        for (i = 0; i < len / 8; ++i) {
            mask = 0xff | (mask << 8);
        }
        if (len % 8 != 0)
            mask = mask | ((len % 8) << i);
        addr->sin_addr.s_addr = mask;
    } else if (hba->addr.ss_family == AF_INET6) {
        if (len > 128)
            return -1;
        unsigned int i;
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&hba->mask;
        for (i = 0; i < len / 8; ++i) {
            addr->sin6_addr.s6_addr[i] = 0xff;
        }
        if (len % 8 != 0)
            addr->sin6_addr.s6_addr[i] = len % 8;
    }
}

static int
od_hba_reader_item(od_config_reader_t *reader) {
    od_config_t *config = reader->config;

    od_config_hba_t *hba;
    hba = od_config_hba_add(config);
    if (hba == NULL)
        return -1;

    void *connection_type = NULL;
    od_config_hba_conn_type_t conn_type;
	int rc;
    rc = od_hba_reader_value(reader, &connection_type);
	if (rc == OD_PARSER_STRING)
		return -1;
	od_keyword_t *keyword = (od_keyword_t *)connection_type;
	switch (keyword->id) {
		case OD_LLOCAL:
			conn_type = OD_CONFIG_HBA_LOCAL;
			break;
		case OD_LHOST:
			conn_type = OD_CONFIG_HBA_HOST;
			break;
		default:
			return -1;
	}

//    if (strcmp(connection_type, "local") == 0) {
//        conn_type = OD_CONFIG_HBA_LOCAL;
//    } else if (strcmp(connection_type, "host") == 0) {
//        conn_type = OD_CONFIG_HBA_HOST;
//    } else {
//        return -1;
//    }

    hba->connection_type = conn_type;

    if (!od_hba_reader_string(reader, &hba->database))
        return -1;

    if (!od_hba_reader_string(reader, &hba->user))
        return -1;

    if (conn_type != OD_CONFIG_HBA_LOCAL) {
        char *address = NULL;
        char *mask = NULL;

        // ip addr
        if (!od_hba_reader_string(reader, &address))
            return -1;
        mask = strchr(address, '/');
        if (mask)
            *mask++ = 0;

        if (od_hba_reader_address(&hba->addr, address) == -1)
            return -1;

        // mask
        if (mask)
            od_hba_reader_prefix(hba, mask);
        else {
            if (!od_hba_reader_string(reader, &address))
                return -1;
            od_hba_reader_address(&hba->mask, address);
        }
    }
}

static int
od_hba_reader_parse(od_config_reader_t *reader) {
    int rc;
    for (;;) {
        rc = od_hba_reader_item(reader);
        if (rc == -1)
            break;
    }
}

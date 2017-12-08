
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <machinarium.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/parser.h"
#include "sources/config.h"

enum
{
	OD_LYES,
	OD_LNO,
	OD_LINCLUDE,
	OD_LDAEMONIZE,
	OD_LLOG_TO_STDOUT,
	OD_LLOG_DEBUG,
	OD_LLOG_CONFIG,
	OD_LLOG_SESSION,
	OD_LLOG_QUERY,
	OD_LLOG_FILE,
	OD_LLOG_FORMAT,
	OD_LLOG_STATS,
	OD_LPID_FILE,
	OD_LLOG_SYSLOG,
	OD_LLOG_SYSLOG_IDENT,
	OD_LLOG_SYSLOG_FACILITY,
	OD_LSTATS_INTERVAL,
	OD_LLISTEN,
	OD_LHOST,
	OD_LPORT,
	OD_LBACKLOG,
	OD_LNODELAY,
	OD_LKEEPALIVE,
	OD_LREADAHEAD,
	OD_LPIPELINING,
	OD_LWORKERS,
	OD_LCLIENT_MAX,
	OD_LCLIENT_FWD_ERROR,
	OD_LTLS,
	OD_LTLS_CA_FILE,
	OD_LTLS_KEY_FILE,
	OD_LTLS_CERT_FILE,
	OD_LTLS_PROTOCOLS,
	OD_LSTORAGE,
	OD_LTYPE,
	OD_LDEFAULT,
	OD_LDATABASE,
	OD_LUSER,
	OD_LPASSWORD,
	OD_LPOOL,
	OD_LPOOL_SIZE,
	OD_LPOOL_TIMEOUT,
	OD_LPOOL_TTL,
	OD_LPOOL_CANCEL,
	OD_LPOOL_ROLLBACK,
	OD_LSTORAGE_DB,
	OD_LSTORAGE_USER,
	OD_LSTORAGE_PASSWORD,
	OD_LAUTHENTICATION,
	OD_LAUTH_QUERY,
	OD_LAUTH_QUERY_DB,
	OD_LAUTH_QUERY_USER
};

typedef struct
{
	od_parser_t  parser;
	od_scheme_t *scheme;
	od_error_t  *error;
	char        *config_file;
	char        *data;
	int          data_size;
	uint64_t     version;
} od_config_t;

static od_keyword_t od_config_keywords[] =
{
	/* main */
	od_keyword("yes",                 OD_LYES),
	od_keyword("no",                  OD_LNO),
	od_keyword("include",             OD_LINCLUDE),
	od_keyword("daemonize",           OD_LDAEMONIZE),
	od_keyword("pid_file",            OD_LPID_FILE),
	od_keyword("log_debug",           OD_LLOG_DEBUG),
	od_keyword("log_to_stdout",       OD_LLOG_TO_STDOUT),
	od_keyword("log_config",          OD_LLOG_CONFIG),
	od_keyword("log_session",         OD_LLOG_SESSION),
	od_keyword("log_query",           OD_LLOG_QUERY),
	od_keyword("log_file",            OD_LLOG_FILE),
	od_keyword("log_format",          OD_LLOG_FORMAT),
	od_keyword("log_stats",           OD_LLOG_STATS),
	od_keyword("log_syslog",          OD_LLOG_SYSLOG),
	od_keyword("log_syslog_ident",    OD_LLOG_SYSLOG_IDENT),
	od_keyword("log_syslog_facility", OD_LLOG_SYSLOG_FACILITY),
	od_keyword("stats_interval",      OD_LSTATS_INTERVAL),
	/* listen */
	od_keyword("listen",              OD_LLISTEN),
	od_keyword("host",                OD_LHOST),
	od_keyword("port",                OD_LPORT),
	od_keyword("backlog",             OD_LBACKLOG),
	od_keyword("nodelay",             OD_LNODELAY),
	od_keyword("keepalive",           OD_LKEEPALIVE),
	od_keyword("readahead",           OD_LREADAHEAD),
	od_keyword("pipelining",          OD_LPIPELINING),
	od_keyword("workers",             OD_LWORKERS),
	od_keyword("client_max",          OD_LCLIENT_MAX),
	od_keyword("client_fwd_error",    OD_LCLIENT_FWD_ERROR),
	od_keyword("tls",                 OD_LTLS),
	od_keyword("tls_ca_file",         OD_LTLS_CA_FILE),
	od_keyword("tls_key_file",        OD_LTLS_KEY_FILE),
	od_keyword("tls_cert_file",       OD_LTLS_CERT_FILE),
	od_keyword("tls_protocols",       OD_LTLS_PROTOCOLS),
	/* storage */
	od_keyword("storage",             OD_LSTORAGE),
	od_keyword("type",                OD_LTYPE),
	od_keyword("default",             OD_LDEFAULT),
	/* database */
	od_keyword("database",            OD_LDATABASE),
	od_keyword("user",                OD_LUSER),
	od_keyword("password",            OD_LPASSWORD),
	od_keyword("pool",                OD_LPOOL),
	od_keyword("pool_size",           OD_LPOOL_SIZE),
	od_keyword("pool_timeout",        OD_LPOOL_TIMEOUT),
	od_keyword("pool_ttl",            OD_LPOOL_TTL),
	od_keyword("pool_cancel",         OD_LPOOL_CANCEL),
	od_keyword("pool_rollback",       OD_LPOOL_ROLLBACK),
	od_keyword("storage_db",          OD_LSTORAGE_DB),
	od_keyword("storage_user",        OD_LSTORAGE_USER),
	od_keyword("storage_password",    OD_LSTORAGE_PASSWORD),
	od_keyword("authentication",      OD_LAUTHENTICATION),
	od_keyword("auth_query",          OD_LAUTH_QUERY),
	od_keyword("auth_query_db",       OD_LAUTH_QUERY_DB),
	od_keyword("auth_query_user",     OD_LAUTH_QUERY_USER),
	{ 0, 0, 0 }
};

static int
od_config_open(od_config_t *config, char *config_file)
{
	config->config_file = config_file;
	/* read file */
	struct stat st;
	int rc = lstat(config_file, &st);
	if (rc == -1)
		goto error;
	char *config_buf = malloc(st.st_size);
	if (config_buf == NULL)
		goto error;
	FILE *file = fopen(config_file, "r");
	if (file == NULL) {
		free(config_buf);
		goto error;
	}
	rc = fread(config_buf, st.st_size, 1, file);
	fclose(file);
	if (rc != 1) {
		free(config_buf);
		goto error;
	}
	config->data = config_buf;
	config->data_size = st.st_size;
	od_parser_init(&config->parser, config->data, config->data_size);
	return 0;
error:
	od_errorf(config->error, "failed to open config file '%s'",
	          config_file);
	return -1;
}

static void
od_config_close(od_config_t *config)
{
	free(config->data);
}

static void
od_config_error(od_config_t *config, od_token_t *token, char *fmt, ...)
{
	char msg[256];
	va_list args;
	va_start(args, fmt);
	od_vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);
	int line = config->parser.line;
	if (token)
		line = token->line;
	od_errorf(config->error, "%s:%d %s", config->config_file,
	          line, msg);
}

static bool
od_config_next_is(od_config_t *config, int id)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(&config->parser, &token);
	od_parser_push(&config->parser, &token);
	if (rc != id)
		return false;
	return true;
}

static bool
od_config_next_keyword(od_config_t *config, od_keyword_t *keyword)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(&config->parser, &token);
	if (rc != OD_PARSER_KEYWORD)
		goto error;
	od_keyword_t *match;
	match = od_keyword_match(od_config_keywords, &token);
	if (keyword == NULL)
		goto error;
	if (keyword != match)
		goto error;
	return true;
error:
	od_parser_push(&config->parser, &token);
	od_config_error(config, &token, "expected '%s'", keyword->name);
	return false;
}

static bool
od_config_next_symbol(od_config_t *config, char symbol)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(&config->parser, &token);
	if (rc != OD_PARSER_SYMBOL)
		goto error;
	if (token.value.num != (uint64_t)symbol)
		goto error;
	return true;
error:
	od_parser_push(&config->parser, &token);
	od_config_error(config, &token, "expected '%c'", symbol);
	return false;
}

static bool
od_config_next_string(od_config_t *config, char **value)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(&config->parser, &token);
	if (rc != OD_PARSER_STRING) {
		od_parser_push(&config->parser, &token);
		od_config_error(config, &token, "expected 'string'");
		return false;
	}
	char *copy = malloc(token.value.string.size + 1);
	if (copy == NULL) {
		od_parser_push(&config->parser, &token);
		od_config_error(config, &token, "memory allocation error");
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
od_config_next_number(od_config_t *config, int *number)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(&config->parser, &token);
	if (rc != OD_PARSER_NUM) {
		od_parser_push(&config->parser, &token);
		od_config_error(config, &token, "expected 'number'");
		return false;
	}
	/* uint64 to int conversion */
	*number = token.value.num;
	return true;
}

static bool
od_config_next_yes_no(od_config_t *config, int *value)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(&config->parser, &token);
	if (rc != OD_PARSER_KEYWORD)
		goto error;
	od_keyword_t *keyword;
	keyword = od_keyword_match(od_config_keywords, &token);
	if (keyword == NULL)
		goto error;
	switch (keyword->id) {
	case OD_LYES:
		*value = 1;
		break;
	case OD_LNO:
		*value = 0;
		break;
	default:
		goto error;
	}
	return true;
error:
	od_parser_push(&config->parser, &token);
	od_config_error(config, &token, "expected 'yes/no'");
	return false;
}

static int
od_config_parse_listen(od_config_t *config)
{
	od_scheme_t *scheme = config->scheme;

	od_schemelisten_t *listen;
	listen = od_schemelisten_add(scheme);
	if (listen == NULL) {
		return -1;
	}

	/* { */
	if (! od_config_next_symbol(config, '{'))
		return -1;

	for (;;)
	{
		od_token_t token;
		int rc;
		rc = od_parser_next(&config->parser, &token);
		switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
			od_config_error(config, &token, "unexpected end of config file");
			return -1;
		case OD_PARSER_SYMBOL:
			/* } */
			if (token.value.num == '}')
				return 0;
			/* fall through */
		default:
			od_config_error(config, &token, "incorrect or unexpected parameter");
			return -1;
		}
		od_keyword_t *keyword;
		keyword = od_keyword_match(od_config_keywords, &token);
		if (keyword == NULL) {
			od_config_error(config, &token, "unknown parameter");
			return -1;
		}
		switch (keyword->id) {
		/* host */
		case OD_LHOST:
			if (! od_config_next_string(config, &listen->host))
				return -1;
			continue;
		/* port */
		case OD_LPORT:
			if (! od_config_next_number(config, &listen->port))
				return -1;
			continue;
		/* backlog */
		case OD_LBACKLOG:
			if (! od_config_next_number(config, &listen->backlog))
				return -1;
			continue;
		/* tls */
		case OD_LTLS:
			if (! od_config_next_string(config, &listen->tls))
				return -1;
			continue;
		/* tls_ca_file */
		case OD_LTLS_CA_FILE:
			if (! od_config_next_string(config, &listen->tls_ca_file))
				return -1;
			continue;
		/* tls_key_file */
		case OD_LTLS_KEY_FILE:
			if (! od_config_next_string(config, &listen->tls_key_file))
				return -1;
			continue;
		/* tls_cert_file */
		case OD_LTLS_CERT_FILE:
			if (! od_config_next_string(config, &listen->tls_cert_file))
				return -1;
			continue;
		/* tls_protocols */
		case OD_LTLS_PROTOCOLS:
			if (! od_config_next_string(config, &listen->tls_protocols))
				return -1;
			continue;
		default:
			od_config_error(config, &token, "unexpected parameter");
			return -1;
		}
	}
	/* unreach */
	return -1;
}

static int
od_config_parse_storage(od_config_t *config)
{
	od_schemestorage_t *storage;
	storage = od_schemestorage_add(config->scheme);
	if (storage == NULL)
		return -1;
	/* name */
	if (! od_config_next_string(config, &storage->name))
		return -1;
	/* { */
	if (! od_config_next_symbol(config, '{'))
		return -1;

	for (;;)
	{
		od_token_t token;
		int rc;
		rc = od_parser_next(&config->parser, &token);
		switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
			od_config_error(config, &token, "unexpected end of config file");
			return -1;
		case OD_PARSER_SYMBOL:
			/* } */
			if (token.value.num == '}')
				return 0;
			/* fall through */
		default:
			od_config_error(config, &token, "incorrect or unexpected parameter");
			return -1;
		}
		od_keyword_t *keyword;
		keyword = od_keyword_match(od_config_keywords, &token);
		if (keyword == NULL) {
			od_config_error(config, &token, "unknown parameter");
			return -1;
		}
		switch (keyword->id) {
		/* type */
		case OD_LTYPE:
			if (! od_config_next_string(config, &storage->type))
				return -1;
			continue;
		/* host */
		case OD_LHOST:
			if (! od_config_next_string(config, &storage->host))
				return -1;
			continue;
		/* port */
		case OD_LPORT:
			if (! od_config_next_number(config, &storage->port))
				return -1;
			continue;
		/* tls */
		case OD_LTLS:
			if (! od_config_next_string(config, &storage->tls))
				return -1;
			continue;
		/* tls_ca_file */
		case OD_LTLS_CA_FILE:
			if (! od_config_next_string(config, &storage->tls_ca_file))
				return -1;
			continue;
		/* tls_key_file */
		case OD_LTLS_KEY_FILE:
			if (! od_config_next_string(config, &storage->tls_key_file))
				return -1;
			continue;
		/* tls_cert_file */
		case OD_LTLS_CERT_FILE:
			if (! od_config_next_string(config, &storage->tls_cert_file))
				return -1;
			continue;
		/* tls_protocols */
		case OD_LTLS_PROTOCOLS:
			if (! od_config_next_string(config, &storage->tls_protocols))
				return -1;
			continue;
		default:
			od_config_error(config, &token, "unexpected parameter");
			return -1;
		}
	}
	/* unreach */
	return -1;
}

static int
od_config_parse_route(od_config_t *config, char *db_name, int db_name_len,
                      int db_is_default)
{
	char *user_name = NULL;
	int   user_name_len = 0;
	int   user_is_default = 0;

	/* user name or default */
	if (od_config_next_is(config, OD_PARSER_STRING)) {
		if (! od_config_next_string(config, &user_name))
			return -1;
	} else {
		if (! od_config_next_keyword(config, &od_config_keywords[OD_LDEFAULT]))
			return -1;
		user_is_default = 1;
		user_name = strdup("default");
		if (user_name == NULL)
			return -1;
	}
	user_name_len = strlen(user_name);

	/* ensure route does not exists and add new route */
	od_schemeroute_t *route;
	route = od_schemeroute_match(config->scheme, db_name, user_name);
	if (route) {
		od_errorf(config->error, "route '%s.%s': is redefined",
		          db_name, user_name);
		free(user_name);
		return -1;
	}
	route = od_schemeroute_add(config->scheme, config->version);
	if (route == NULL) {
		free(user_name);
		return -1;
	}
	route->user_is_default = user_is_default;
	route->user_name_len = user_name_len;
	route->user_name = strdup(user_name);
	free(user_name);
	if (route->user_name == NULL)
		return -1;
	route->db_is_default = db_is_default;
	route->db_name_len = db_name_len;
	route->db_name = strdup(db_name);
	if (route->db_name == NULL)
		return -1;

	/* { */
	if (! od_config_next_symbol(config, '{'))
		return -1;

	for (;;)
	{
		od_token_t token;
		int rc;
		rc = od_parser_next(&config->parser, &token);
		switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
			od_config_error(config, &token, "unexpected end of config file");
			return -1;
		case OD_PARSER_SYMBOL:
			/* } */
			if (token.value.num == '}')
				return 0;
			/* fall through */
		default:
			od_config_error(config, &token, "incorrect or unexpected parameter");
			return -1;
		}
		od_keyword_t *keyword;
		keyword = od_keyword_match(od_config_keywords, &token);
		if (keyword == NULL) {
			od_config_error(config, &token, "unknown parameter");
			return -1;
		}
		switch (keyword->id) {
		/* authentication */
		case OD_LAUTHENTICATION:
			if (! od_config_next_string(config, &route->auth))
				return -1;
			break;
		/* auth_query */
		case OD_LAUTH_QUERY:
			if (! od_config_next_string(config, &route->auth_query))
				return -1;
			break;
		/* auth_query_db */
		case OD_LAUTH_QUERY_DB:
			if (! od_config_next_string(config, &route->auth_query_db))
				return -1;
			break;
		/* auth_query_user */
		case OD_LAUTH_QUERY_USER:
			if (! od_config_next_string(config, &route->auth_query_user))
				return -1;
			break;
		/* password */
		case OD_LPASSWORD:
			if (! od_config_next_string(config, &route->password))
				return -1;
			route->password_len = strlen(route->password);
			continue;
		/* storage */
		case OD_LSTORAGE:
			if (! od_config_next_string(config, &route->storage_name))
				return -1;
			continue;
		/* client_max */
		case OD_LCLIENT_MAX:
			if (! od_config_next_number(config, &route->client_max))
				return -1;
			route->client_max_set = 1;
			continue;
		/* client_fwd_error */
		case OD_LCLIENT_FWD_ERROR:
			if (! od_config_next_yes_no(config, &route->client_fwd_error))
				return -1;
			continue;
		/* pool */
		case OD_LPOOL:
			if (! od_config_next_string(config, &route->pool_sz))
				return -1;
			continue;
		/* pool_size */
		case OD_LPOOL_SIZE:
			if (! od_config_next_number(config, &route->pool_size))
				return -1;
			continue;
		/* pool_timeout */
		case OD_LPOOL_TIMEOUT:
			if (! od_config_next_number(config, &route->pool_timeout))
				return -1;
			continue;
		/* pool_ttl */
		case OD_LPOOL_TTL:
			if (! od_config_next_number(config, &route->pool_ttl))
				return -1;
			continue;
		/* storage_database */
		case OD_LSTORAGE_DB:
			if (! od_config_next_string(config, &route->storage_db))
				return -1;
			continue;
		/* storage_user */
		case OD_LSTORAGE_USER:
			if (! od_config_next_string(config, &route->storage_user))
				return -1;
			route->storage_user_len = strlen(route->storage_user);
			continue;
		/* storage_password */
		case OD_LSTORAGE_PASSWORD:
			if (! od_config_next_string(config, &route->storage_password))
				return -1;
			route->storage_password_len = strlen(route->storage_password);
			continue;
		/* pool_cancel */
		case OD_LPOOL_CANCEL:
			if (! od_config_next_yes_no(config, &route->pool_cancel))
				return -1;
			continue;
		/* pool_rollback */
		case OD_LPOOL_ROLLBACK:
			if (! od_config_next_yes_no(config, &route->pool_rollback))
				return -1;
			continue;
		/* log_debug */
		case OD_LLOG_DEBUG:
			if (! od_config_next_yes_no(config, &route->log_debug))
				return -1;
			continue;
		default:
			od_config_error(config, &token, "unexpected parameter");
			return -1;
		}
	}

	/* unreach */
	return -1;
}

static int
od_config_parse_database(od_config_t *config)
{
	char *db_name = NULL;
	int   db_name_len = 0;
	int   db_is_default = 0;

	/* name or default */
	if (od_config_next_is(config, OD_PARSER_STRING)) {
		if (! od_config_next_string(config, &db_name))
			return -1;
	} else {
		if (! od_config_next_keyword(config, &od_config_keywords[OD_LDEFAULT]))
			return -1;
		db_is_default = 1;
		db_name = strdup("default");
		if (db_name == NULL)
			return -1;
	}
	db_name_len = strlen(db_name);

	/* { */
	if (! od_config_next_symbol(config, '{'))
		goto error;

	for (;;)
	{
		od_token_t token;
		int rc;
		rc = od_parser_next(&config->parser, &token);
		switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
			od_config_error(config, &token, "unexpected end of config file");
			goto error;
		case OD_PARSER_SYMBOL:
			/* } */
			if (token.value.num == '}') {
				/* make sure that db.default is defined */
				od_schemeroute_t *route;
				route = od_schemeroute_match(config->scheme, db_name, "default");
				if (! route) {
					od_errorf(config->error, "route '%s.default': is not defined",
					          db_name);
					free(db_name);
					return -1;
				}
				free(db_name);
				return 0;
			}
			/* fall through */
		default:
			od_config_error(config, &token, "incorrect or unexpected parameter");
			goto error;
		}
		od_keyword_t *keyword;
		keyword = od_keyword_match(od_config_keywords, &token);
		if (keyword == NULL) {
			od_config_error(config, &token, "unknown parameter");
			goto error;
		}
		switch (keyword->id) {
		/* user */
		case OD_LUSER:
			rc = od_config_parse_route(config, db_name, db_name_len, db_is_default);
			if (rc == -1)
				goto error;
			continue;
		default:
			od_config_error(config, &token, "unexpected parameter");
			goto error;
		}
	}
	/* unreach */
	return -1;
error:
	free(db_name);
	return -1;
}

static int
od_config_parse(od_config_t *config)
{
	od_scheme_t *scheme = config->scheme;
	for (;;)
	{
		od_token_t token;
		int rc;
		rc = od_parser_next(&config->parser, &token);
		switch (rc) {
		case OD_PARSER_EOF:
			return 0;
		case OD_PARSER_KEYWORD:
			break;
		default:
			od_config_error(config, &token, "incorrect or unexpected parameter");
			return -1;
		}
		od_keyword_t *keyword;
		keyword = od_keyword_match(od_config_keywords, &token);
		if (keyword == NULL) {
			od_config_error(config, &token, "unknown parameter");
			return -1;
		}
		switch (keyword->id) {
		/* include */
		case OD_LINCLUDE:
		{
			char *config_file;
			if (! od_config_next_string(config, &config_file))
				return -1;
			rc = od_config_load(config->scheme, config->error,
			                    config_file,
			                    config->version);
			free(config_file);
			if (rc == -1)
				return -1;
			continue;
		}
		/* daemonize */
		case OD_LDAEMONIZE:
			if (! od_config_next_yes_no(config, &scheme->daemonize))
				return -1;
			continue;
		/* pid_file */
		case OD_LPID_FILE:
			if (! od_config_next_string(config, &scheme->pid_file))
				return -1;
			continue;
		/* log_debug */
		case OD_LLOG_DEBUG:
			if (! od_config_next_yes_no(config, &scheme->log_debug))
				return -1;
			continue;
		/* log_stdout */
		case OD_LLOG_TO_STDOUT:
			if (! od_config_next_yes_no(config, &scheme->log_to_stdout))
				return -1;
			continue;
		/* log_config */
		case OD_LLOG_CONFIG:
			if (! od_config_next_yes_no(config, &scheme->log_config))
				return -1;
			continue;
		/* log_session */
		case OD_LLOG_SESSION:
			if (! od_config_next_yes_no(config, &scheme->log_session))
				return -1;
			continue;
		/* log_query */
		case OD_LLOG_QUERY:
			if (! od_config_next_yes_no(config, &scheme->log_query))
				return -1;
			continue;
		/* log_stats */
		case OD_LLOG_STATS:
			if (! od_config_next_yes_no(config, &scheme->log_stats))
				return -1;
			continue;
		/* log_format */
		case OD_LLOG_FORMAT:
			if (! od_config_next_string(config, &scheme->log_format))
				return -1;
			continue;
		/* log_file */
		case OD_LLOG_FILE:
			if (! od_config_next_string(config, &scheme->log_file))
				return -1;
			continue;
		/* log_syslog */
		case OD_LLOG_SYSLOG:
			if (! od_config_next_yes_no(config, &scheme->log_syslog))
				return -1;
			continue;
		/* log_syslog_ident */
		case OD_LLOG_SYSLOG_IDENT:
			if (! od_config_next_string(config, &scheme->log_syslog_ident))
				return -1;
			continue;
		/* log_syslog_facility */
		case OD_LLOG_SYSLOG_FACILITY:
			if (! od_config_next_string(config, &scheme->log_syslog_facility))
				return -1;
			continue;
		/* stats_interval */
		case OD_LSTATS_INTERVAL:
			if (! od_config_next_number(config, &scheme->stats_interval))
				return -1;
			continue;
		/* client_max */
		case OD_LCLIENT_MAX:
			if (! od_config_next_number(config, &scheme->client_max))
				return -1;
			scheme->client_max_set = 1;
			continue;
		/* pipelining */
		case OD_LPIPELINING:
			if (! od_config_next_number(config, &scheme->server_pipelining))
				return -1;
			continue;
		/* readahead */
		case OD_LREADAHEAD:
			if (! od_config_next_number(config, &scheme->readahead))
				return -1;
			continue;
		/* nodelay */
		case OD_LNODELAY:
			if (! od_config_next_yes_no(config, &scheme->nodelay))
				return -1;
			continue;
		/* keepalive */
		case OD_LKEEPALIVE:
			if (! od_config_next_number(config, &scheme->keepalive))
				return -1;
			continue;
		/* workers */
		case OD_LWORKERS:
			if (! od_config_next_number(config, &scheme->workers))
				return -1;
			continue;
		/* listen */
		case OD_LLISTEN:
			rc = od_config_parse_listen(config);
			if (rc == -1)
				return -1;
			continue;
		/* storage */
		case OD_LSTORAGE:
			rc = od_config_parse_storage(config);
			if (rc == -1)
				return -1;
			continue;
		/* database */
		case OD_LDATABASE:
			rc = od_config_parse_database(config);
			if (rc == -1)
				return -1;
			continue;
		default:
			od_config_error(config, &token, "unexpected parameter");
			return -1;
		}
	}
	/* unreach */
	return -1;
}

int od_config_load(od_scheme_t *scheme, od_error_t *error, char *config_file,
                   uint64_t version)
{
	od_config_t config;
	memset(&config, 0, sizeof(config));
	config.error   = error;
	config.scheme  = scheme;
	config.version = version;
	int rc;
	rc = od_config_open(&config, config_file);
	if (rc == -1)
		return -1;
	rc = od_config_parse(&config);
	od_config_close(&config);
	return rc;
}

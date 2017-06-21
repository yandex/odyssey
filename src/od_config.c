
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <machinarium.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_id.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"

#define od_keyword(name, token) { name, sizeof(name) - 1, token }

static od_keyword_t od_config_keywords[] =
{
	/* main */
	od_keyword("yes",             OD_LYES),
	od_keyword("no",              OD_LNO),
	od_keyword("on",              OD_LON),
	od_keyword("off",             OD_LOFF),
	od_keyword("daemonize",       OD_LDAEMONIZE),
	od_keyword("log_debug",       OD_LLOG_DEBUG),
	od_keyword("log_config",      OD_LLOG_CONFIG),
	od_keyword("log_file",        OD_LLOG_FILE),
	od_keyword("pid_file",        OD_LPID_FILE),
	od_keyword("syslog",          OD_LSYSLOG),
	od_keyword("syslog_ident",    OD_LSYSLOG_IDENT),
	od_keyword("syslog_facility", OD_LSYSLOG_FACILITY),
	od_keyword("stats_period",    OD_LSTATS_PERIOD),
	od_keyword("pooling",         OD_LPOOLING),
	/* listen */
	od_keyword("listen",          OD_LLISTEN),
	od_keyword("host",            OD_LHOST),
	od_keyword("port",            OD_LPORT),
	od_keyword("backlog",         OD_LBACKLOG),
	od_keyword("nodelay",         OD_LNODELAY),
	od_keyword("keepalive",       OD_LKEEPALIVE),
	od_keyword("readahead",       OD_LREADAHEAD),
	od_keyword("pipelining",      OD_LPIPELINING),
	od_keyword("workers",         OD_LWORKERS),
	od_keyword("client_max",      OD_LCLIENT_MAX),
	od_keyword("tls_mode",        OD_LTLS_MODE),
	od_keyword("tls_ca_file",     OD_LTLS_CA_FILE),
	od_keyword("tls_key_file",    OD_LTLS_KEY_FILE),
	od_keyword("tls_cert_file",   OD_LTLS_CERT_FILE),
	od_keyword("tls_protocols",   OD_LTLS_PROTOCOLS),
	/* server */
	od_keyword("storage",         OD_LSTORAGE),
	/* route */
	od_keyword("route",           OD_LROUTE),
	od_keyword("default",         OD_LDEFAULT),
	od_keyword("database",        OD_LDATABASE),
	od_keyword("user",            OD_LUSER),
	od_keyword("password",        OD_LPASSWORD),
	od_keyword("ttl",             OD_LTTL),
	od_keyword("cancel",          OD_LCANCEL),
	od_keyword("discard",         OD_LDISCARD),
	od_keyword("rollback",        OD_LROLLBACK),
	od_keyword("pool_size",       OD_LPOOL_SIZE),
	od_keyword("pool_timeout",    OD_LPOOL_TIMEOUT),
	/* user */
	od_keyword("authentication",  OD_LAUTHENTICATION),
	od_keyword("user",            OD_LUSER),
	od_keyword("deny",            OD_LDENY),
	{ NULL, 0,  0 }
};

void
od_config_init(od_config_t *config, od_log_t *log,
               od_scheme_t *scheme)
{
	od_lex_init(&config->lex);
	config->log = log;
	config->scheme = scheme;
}

int
od_config_open(od_config_t *config, char *file)
{
	/* read file */
	struct stat st;
	int rc = lstat(file, &st);
	if (rc == -1) {
		od_error(config->log, "config", "failed to open config file '%s'",
		         file);
		return -1;
	}
	char *config_buf = malloc(st.st_size);
	if (config_buf == NULL) {
		od_error(config->log, "config", "memory allocation error");
		return -1;
	}
	FILE *f = fopen(file, "r");
	if (f == NULL) {
		free(config_buf);
		od_error(config->log, "config", "failed to open config file '%s'", file);
		return -1;
	}
	rc = fread(config_buf, st.st_size, 1, f);
	fclose(f);
	if (rc != 1) {
		free(config_buf);
		od_error(config->log, "config", "failed to open config file '%s'", file);
		return -1;
	}
	od_lex_open(&config->lex, od_config_keywords, config_buf,
	            st.st_size);
	config->scheme->config_file = file;
	return 0;
}

void
od_config_close(od_config_t *config)
{
	od_lex_free(&config->lex);
}

static void
od_config_error(od_config_t *config, od_token_t *tk, char *fmt, ...)
{
	char msg[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);
	int line = config->lex.line;
	if (tk)
		line = tk->line;
	od_error(config->log, "config", "%s:%d %s",
	         config->scheme->config_file, line, msg);
}

static int
od_config_next(od_config_t *config, int id, od_token_t **tk)
{
	od_token_t *tkp = NULL;
	int token = od_lex_pop(&config->lex, &tkp);
	if (token == OD_LERROR) {
		od_config_error(config, NULL, "%s", config->lex.error);
		return -1;
	}
	if (tk) {
		*tk = tkp;
	}
	if (token != id) {
		if (id < 0xff && ispunct(id)) {
			od_config_error(config, tkp, "expected '%c'", id);
			return -1;
		}
		od_config_error(config, tkp, "expected '%s'",
		                od_lex_name_of(&config->lex, id));
		return -1;
	}
	return 0;
}

static int
od_config_next_yes_no(od_config_t *config, od_token_t **tk)
{
	int rc;
	rc = od_lex_pop(&config->lex, tk);
	if (rc == OD_LYES)
		return 1;
	if (rc == OD_LNO)
		return 0;
	od_config_error(config, *tk, "expected yes/no");
	return -1;
}

static int
od_config_parse_listen(od_config_t *config)
{
	if (od_config_next(config, '{', NULL) == -1)
		return -1;
	od_token_t *tk;
	int rc;
	int eof = 0;
	while (! eof)
	{
		rc = od_lex_pop(&config->lex, &tk);
		switch (rc) {
		/* host */
		case OD_LHOST:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->host = tk->v.string;
			continue;
		/* port */
		case OD_LPORT:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->port = tk->v.num;
			continue;
		/* backlog */
		case OD_LBACKLOG:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->backlog = tk->v.num;
			continue;
		/* nodelay */
		case OD_LNODELAY:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			config->scheme->nodelay = rc;
			continue;
		/* keepalive */
		case OD_LKEEPALIVE:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->keepalive = tk->v.num;
			continue;
		/* tls_mode */
		case OD_LTLS_MODE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->tls_mode = tk->v.string;
			continue;
		/* tls_ca_file */
		case OD_LTLS_CA_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->tls_ca_file = tk->v.string;
			continue;
		/* tls_key_file */
		case OD_LTLS_KEY_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->tls_key_file = tk->v.string;
			continue;
		/* tls_cert_file */
		case OD_LTLS_CERT_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->tls_cert_file = tk->v.string;
			continue;
		/* tls_protocols */
		case OD_LTLS_PROTOCOLS:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->tls_protocols = tk->v.string;
			continue;
		case OD_LEOF:
			od_config_error(config, tk, "unexpected end of config file");
			return -1;
		case '}':
			eof = 1;
			continue;
		default:
			od_config_error(config, tk, "unknown option");
			return -1;
		}
	}
	return 0;
}

static int
od_config_parse_storage(od_config_t *config)
{
	od_schemestorage_t *storage;
	storage = od_schemestorage_add(config->scheme);
	if (storage == NULL)
		return -1;
	od_token_t *tk;
	int rc;
	/* name */
	if (od_config_next(config, OD_LSTRING, &tk) == -1)
		return -1;
	storage->name = tk->v.string;
	if (od_config_next(config, '{', NULL) == -1)
		return -1;
	int eof = 0;
	while (! eof)
	{
		rc = od_lex_pop(&config->lex, &tk);
		switch (rc) {
		/* host */
		case OD_LHOST:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->host = tk->v.string;
			continue;
		/* port */
		case OD_LPORT:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			storage->port = tk->v.num;
			continue;
		/* tls_mode */
		case OD_LTLS_MODE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->tls_mode = tk->v.string;
			continue;
		/* tls_ca_file */
		case OD_LTLS_CA_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->tls_ca_file = tk->v.string;
			continue;
		/* tls_key_file */
		case OD_LTLS_KEY_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->tls_key_file = tk->v.string;
			continue;
		/* tls_cert_file */
		case OD_LTLS_CERT_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->tls_cert_file = tk->v.string;
			continue;
		/* tls_protocols */
		case OD_LTLS_PROTOCOLS:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->tls_protocols = tk->v.string;
			continue;
		case OD_LEOF:
			od_config_error(config, tk, "unexpected end of config file");
			return -1;
		case '}':
			eof = 1;
			continue;
		default:
			od_config_error(config, tk, "unknown option");
			return -1;
		}
	}
	return 0;
}

static int
od_config_parse_route(od_config_t *config)
{
	/* "name" or default */
	od_token_t *name;
	int rc;
	rc = od_lex_pop(&config->lex, &name);
	switch (rc) {
	case OD_LSTRING:
		break;
	case OD_LDEFAULT:
		name = NULL;
		break;
	default:
		od_config_error(config, name, "bad route name");
		return -1;
	}

	od_schemeroute_t *route;
	route = od_schemeroute_add(config->scheme);
	if (route == NULL)
		return -1;
	if (name == NULL) {
		route->is_default = 1;
		route->target = "";
	} else {
		route->target = name->v.string;
	}
	if (od_config_next(config, '{', NULL) == -1)
		return -1;
	od_token_t *tk;
	int eof = 0;
	while (! eof)
	{
		rc = od_lex_pop(&config->lex, &tk);
		switch (rc) {
		/* storage */
		case OD_LSTORAGE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			route->route = tk->v.string;
			continue;
		/* client_max */
		case OD_LCLIENT_MAX:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			route->client_max_set = 1;
			route->client_max = tk->v.num;
			continue;
		/* pool_size */
		case OD_LPOOL_SIZE:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			route->pool_size = tk->v.num;
			continue;
		/* pool_timeout */
		case OD_LPOOL_TIMEOUT:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			route->pool_timeout = tk->v.num;
			continue;
		/* database */
		case OD_LDATABASE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			route->database = tk->v.string;
			continue;
		/* user */
		case OD_LUSER:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			route->user = tk->v.string;
			route->user_len = strlen(route->user);
			continue;
		/* password */
		case OD_LPASSWORD:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			route->password = tk->v.string;
			route->password_len = strlen(route->password);
			continue;
		/* ttl */
		case OD_LTTL:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			route->ttl = tk->v.num;
			continue;
		/* cancel */
		case OD_LCANCEL:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			route->cancel = rc;
			continue;
		/* discard */
		case OD_LDISCARD:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			route->discard = rc;
			continue;
		/* rollback */
		case OD_LROLLBACK:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			route->rollback = rc;
			continue;
		case OD_LEOF:
			od_config_error(config, tk, "unexpected end of config file");
			return -1;
		case '}':
			eof = 1;
			continue;
		default:
			od_config_error(config, tk, "unknown option");
			return -1;
		}
	}
	return 0;
}

static int
od_config_parse_user(od_config_t *config)
{
	/* "name" or default */
	od_token_t *name;
	int rc;
	rc = od_lex_pop(&config->lex, &name);
	switch (rc) {
	case OD_LSTRING:
		break;
	case OD_LDEFAULT:
		name = NULL;
		break;
	default:
		od_config_error(config, name, "bad user name");
		return -1;
	}

	od_schemeuser_t *user;
	user = od_schemeuser_add(config->scheme);
	if (user == NULL)
		return -1;
	if (name == NULL) {
		user->is_default = 1;
		user->user = "";
	} else {
		user->user = name->v.string;
	}
	if (od_config_next(config, '{', NULL) == -1)
		return -1;
	od_token_t *tk;
	int eof = 0;
	while (! eof)
	{
		rc = od_lex_pop(&config->lex, &tk);
		switch (rc) {
		/* authentication */
		case OD_LAUTHENTICATION:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			user->auth = tk->v.string;
			break;
		/* password */
		case OD_LPASSWORD:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			user->password = tk->v.string;
			user->password_len = strlen(user->password);
			continue;
		/* deny */
		case OD_LDENY:
			user->is_deny = 1;
			continue;
		case OD_LEOF:
			od_config_error(config, tk, "unexpected end of config file");
			return -1;
		case '}':
			eof = 1;
			continue;
		default:
			od_config_error(config, tk, "unknown option");
			return -1;
		}
	}
	return 0;
}

int
od_config_parse(od_config_t *config)
{
	od_token_t *tk;
	int rc;
	int eof = 0;
	while (! eof)
	{
		rc = od_lex_pop(&config->lex, &tk);
		switch (rc) {
		/* daemonize */
		case OD_LDAEMONIZE:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			config->scheme->daemonize = rc;
			continue;
		/* log_debug */
		case OD_LLOG_DEBUG:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			config->scheme->log_debug = rc;
			continue;
		/* log_config */
		case OD_LLOG_CONFIG:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			config->scheme->log_config = rc;
			continue;
		/* log_file */
		case OD_LLOG_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->log_file = tk->v.string;
			continue;
		/* pid_file */
		case OD_LPID_FILE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->pid_file = tk->v.string;
			continue;
		/* syslog */
		case OD_LSYSLOG:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			config->scheme->syslog = rc;
			continue;
		/* syslog_ident */
		case OD_LSYSLOG_IDENT:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->syslog_ident = tk->v.string;
			continue;
		/* syslog_facility */
		case OD_LSYSLOG_FACILITY:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->syslog_facility = tk->v.string;
			continue;
		/* stats_period */
		case OD_LSTATS_PERIOD:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->stats_period = tk->v.num;
			continue;
		/* pooling */
		case OD_LPOOLING:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->pooling = tk->v.string;
			continue;
		/* client_max */
		case OD_LCLIENT_MAX:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->client_max = tk->v.num;
			config->scheme->client_max_set = 1;
			continue;
		/* readahead */
		case OD_LREADAHEAD:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->readahead = tk->v.num;
			continue;
		/* pipelining */
		case OD_LPIPELINING:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->server_pipelining = tk->v.num;
			continue;
		/* workers */
		case OD_LWORKERS:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->workers = tk->v.num;
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
		/* route */
		case OD_LROUTE:
			rc = od_config_parse_route(config);
			if (rc == -1)
				return -1;
			continue;
		/* user */
		case OD_LUSER:
			rc = od_config_parse_user(config);
			if (rc == -1)
				return -1;
			continue;
		case OD_LEOF:
			eof = 1;
			continue;
		default:
			od_config_error(config, tk, "unknown option");
			return -1;
		}
	}
	return 0;
}

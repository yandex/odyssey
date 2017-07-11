
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
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

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/syslog.h"
#include "sources/log.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/lex.h"
#include "sources/config.h"

#define od_keyword(name, token) { name, sizeof(name) - 1, token }

static od_keyword_t od_config_keywords[] =
{
	/* main */
	od_keyword("yes",              OD_LYES),
	od_keyword("no",               OD_LNO),
	od_keyword("on",               OD_LON),
	od_keyword("off",              OD_LOFF),
	od_keyword("daemonize",        OD_LDAEMONIZE),
	od_keyword("log_debug",        OD_LLOG_DEBUG),
	od_keyword("log_config",       OD_LLOG_CONFIG),
	od_keyword("log_session",      OD_LLOG_SESSION),
	od_keyword("log_file",         OD_LLOG_FILE),
	od_keyword("log_statistics",   OD_LLOG_STATISTICS),
	od_keyword("pid_file",         OD_LPID_FILE),
	od_keyword("syslog",           OD_LSYSLOG),
	od_keyword("syslog_ident",     OD_LSYSLOG_IDENT),
	od_keyword("syslog_facility",  OD_LSYSLOG_FACILITY),
	/* listen */
	od_keyword("listen",           OD_LLISTEN),
	od_keyword("host",             OD_LHOST),
	od_keyword("port",             OD_LPORT),
	od_keyword("backlog",          OD_LBACKLOG),
	od_keyword("nodelay",          OD_LNODELAY),
	od_keyword("keepalive",        OD_LKEEPALIVE),
	od_keyword("readahead",        OD_LREADAHEAD),
	od_keyword("pipelining",       OD_LPIPELINING),
	od_keyword("workers",          OD_LWORKERS),
	od_keyword("client_max",       OD_LCLIENT_MAX),
	od_keyword("tls",              OD_LTLS),
	od_keyword("tls_ca_file",      OD_LTLS_CA_FILE),
	od_keyword("tls_key_file",     OD_LTLS_KEY_FILE),
	od_keyword("tls_cert_file",    OD_LTLS_CERT_FILE),
	od_keyword("tls_protocols",    OD_LTLS_PROTOCOLS),
	/* storage */
	od_keyword("storage",          OD_LSTORAGE),
	od_keyword("type",             OD_LTYPE),
	/* database */
	od_keyword("database",         OD_LDATABASE),
	od_keyword("user",             OD_LUSER),
	od_keyword("password",         OD_LPASSWORD),
	od_keyword("authentication",   OD_LAUTHENTICATION),
	od_keyword("user",             OD_LUSER),
	od_keyword("storage_db",       OD_LSTORAGE_DB),
	od_keyword("storage_user",     OD_LSTORAGE_USER),
	od_keyword("storage_password", OD_LSTORAGE_PASSWORD),
	od_keyword("pool",             OD_LPOOL),
	od_keyword("pool_size",        OD_LPOOL_SIZE),
	od_keyword("pool_timeout",     OD_LPOOL_TIMEOUT),
	od_keyword("pool_ttl",         OD_LPOOL_TTL),
	od_keyword("pool_cancel",      OD_LPOOL_CANCEL),
	od_keyword("pool_discard",     OD_LPOOL_DISCARD),
	od_keyword("pool_rollback",    OD_LPOOL_ROLLBACK),
	od_keyword("default",          OD_LDEFAULT),

	{ NULL, 0,  0 }
};

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
		/* tls */
		case OD_LTLS:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			config->scheme->tls = tk->v.string;
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
		/* type */
		case OD_LTYPE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->type = tk->v.string;
			continue;
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
		/* tls */
		case OD_LTLS:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			storage->tls = tk->v.string;
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
od_config_parse_user(od_config_t *config, od_schemedb_t *db)
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
	user = od_schemeuser_add(db);
	if (user == NULL)
		return -1;
	if (name == NULL) {
		user->is_default = 1;
		user->user = "default";
	} else {
		user->user = name->v.string;
	}
	user->user_len = strlen(user->user);
	user->db = db;
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
			user->user_password = tk->v.string;
			user->user_password_len = strlen(user->user_password);
			continue;
		/* storage */
		case OD_LSTORAGE:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			user->storage_name = tk->v.string;
			continue;
		/* client_max */
		case OD_LCLIENT_MAX:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			user->client_max_set = 1;
			user->client_max = tk->v.num;
			continue;
		/* pool */
		case OD_LPOOL:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			user->pool_sz = tk->v.string;
			continue;
		/* pool_size */
		case OD_LPOOL_SIZE:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			user->pool_size = tk->v.num;
			continue;
		/* pool_timeout */
		case OD_LPOOL_TIMEOUT:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			user->pool_timeout = tk->v.num;
			continue;
		/* pool_ttl */
		case OD_LPOOL_TTL:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			user->pool_ttl = tk->v.num;
			continue;
		/* storage_database */
		case OD_LSTORAGE_DB:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			user->storage_db = tk->v.string;
			continue;
		/* storage_user */
		case OD_LSTORAGE_USER:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			user->storage_user = tk->v.string;
			user->storage_user_len = strlen(user->storage_user);
			continue;
		/* storage_password */
		case OD_LSTORAGE_PASSWORD:
			if (od_config_next(config, OD_LSTRING, &tk) == -1)
				return -1;
			user->storage_password = tk->v.string;
			user->storage_password_len = strlen(user->storage_password);
			continue;
		/* pool_cancel */
		case OD_LPOOL_CANCEL:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			user->pool_cancel = rc;
			continue;
		/* pool_discard */
		case OD_LPOOL_DISCARD:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			user->pool_discard = rc;
			continue;
		/* pool_rollback */
		case OD_LPOOL_ROLLBACK:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			user->pool_rollback = rc;
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
od_config_parse_database(od_config_t *config)
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
		od_config_error(config, name, "bad database name");
		return -1;
	}

	od_schemedb_t *db;
	db = od_schemedb_add(config->scheme);
	if (db == NULL)
		return -1;
	if (name == NULL) {
		db->is_default = 1;
		db->name = "default";
	} else {
		db->name = name->v.string;
	}
	if (od_config_next(config, '{', NULL) == -1)
		return -1;
	od_token_t *tk;
	int eof = 0;
	while (! eof)
	{
		rc = od_lex_pop(&config->lex, &tk);
		switch (rc) {
		/* user */
		case OD_LUSER:{
			rc = od_config_parse_user(config, db);
			if (rc == -1)
				return -1;
			break;
		}
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

void od_config_init(od_config_t *config, od_log_t *log,
                    od_scheme_t *scheme)
{
	od_lex_init(&config->lex);
	config->log = log;
	config->scheme = scheme;
}

int od_config_open(od_config_t *config, char *file)
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

void od_config_close(od_config_t *config)
{
	od_lex_free(&config->lex);
}

int od_config_parse(od_config_t *config)
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
		/* log_session */
		case OD_LLOG_SESSION:
			rc = od_config_next_yes_no(config, &tk);
			if (rc == -1)
				return -1;
			config->scheme->log_session = rc;
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
		/* log_statistics */
		case OD_LLOG_STATISTICS:
			if (od_config_next(config, OD_LNUMBER, &tk) == -1)
				return -1;
			config->scheme->log_statistics = tk->v.num;
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
		/* database */
		case OD_LDATABASE:
			rc = od_config_parse_database(config);
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

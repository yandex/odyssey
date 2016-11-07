
/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"

#define od_keyword(name, token) { name, sizeof(name) - 1, token }

static odkeyword_t od_config_keywords[] =
{
	/* main */
	od_keyword("odissey",    OD_LODISSEY),
	od_keyword("yes",        OD_LYES),
	od_keyword("no",         OD_LNO),
	od_keyword("on",         OD_LON),
	od_keyword("off",        OD_LOFF),
	od_keyword("daemonize",  OD_LDAEMONIZE),
	od_keyword("logfile",    OD_LLOG_FILE),
	od_keyword("pidfile",    OD_LPID_FILE),
	od_keyword("pooling",    OD_LPOOLING),
	/* listen */
	od_keyword("listen",     OD_LLISTEN),
	od_keyword("host",       OD_LHOST),
	od_keyword("port",       OD_LPORT),
	od_keyword("workers",    OD_LWORKERS),
	od_keyword("client_max", OD_LWORKERS),
	/* server */
	od_keyword("server",     OD_LSERVER),
	od_keyword("default",    OD_LDEFAULT),
	/* routing */
	od_keyword("routing",    OD_LSERVER),
	od_keyword("mode",       OD_LMODE),
	od_keyword("user",       OD_LUSER),
	od_keyword("password",   OD_LPASSWORD),
	od_keyword("pool_min",   OD_LPOOL_MIN),
	od_keyword("pool_max",   OD_LPOOL_MAX),
	{ NULL, 0,  0 }
};

int
od_configopen(odconfig_t *config,
              odlog_t *log,
              odscheme_t *scheme,
              char *file)
{
	/* read file */
	struct stat st;
	int rc = lstat(file, &st);
	if (rc == -1) {
		od_log(log, "error: failed to open config file '%s'",
		       file);
		return -1;
	}
	char *config_buf = malloc(st.st_size);
	if (config_buf == NULL) {
		od_log(log, "error: memory allocation error");
		return -1;
	}
	FILE *f = fopen(file, "r");
	if (f == NULL) {
		free(config_buf);
		od_log(log, "error: failed to open config file '%s'",
		       file);
		return -1;
	}
	rc = fread(config_buf, st.st_size, 1, f);
	fclose(f);
	if (rc != 1) {
		free(config_buf);
		od_log(log, "error: failed to open config file '%s'",
		       file);
		return -1;
	}
	/* init config context */
	scheme->config_file = file;
	config->scheme = scheme;
	config->log = log;
	od_lexinit(&config->lex, od_config_keywords, config_buf,
	           st.st_size);
	return 0;
}

void
od_configclose(odconfig_t *config)
{
	od_lexfree(&config->lex);
}

int
od_configparse(odconfig_t *config)
{
	(void)config;
	return 0;
}

#ifndef OD_CONFIG_H_
#define OD_CONFIG_H_

/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

enum {
	OD_LODISSEY = OD_LCUSTOM,
	OD_LYES,
	OD_LNO,
	OD_LON,
	OD_LOFF,
	OD_LDAEMONIZE,
	OD_LLOG_FILE,
	OD_LPID_FILE,
	OD_LPOOLING,
	OD_LLISTEN,
	OD_LHOST,
	OD_LPORT,
	OD_LWORKERS,
	OD_LCLIENT_MAX,
	OD_LSERVER,
	OD_LDEFAULT,
	OD_LROUTING,
	OD_LMODE,
	OD_LUSER,
	OD_LPASSWORD,
	OD_LPOOL_MIN,
	OD_LPOOL_MAX,
};

typedef struct {
	odlex_t lex;
	odlog_t *log;
	odscheme_t *scheme;
} odconfig_t;

int  od_configopen(odconfig_t*, odlog_t*, odscheme_t*, char*);
void od_configclose(odconfig_t*);
int  od_configparse(odconfig_t*);

#endif

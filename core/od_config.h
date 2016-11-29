#ifndef OD_CONFIG_H_
#define OD_CONFIG_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
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
	OD_LSYSLOG,
	OD_LSYSLOG_IDENT,
	OD_LSYSLOG_FACILITY,
	OD_LPOOLING,
	OD_LLISTEN,
	OD_LHOST,
	OD_LPORT,
	OD_LBACKLOG,
	OD_LNODELAY,
	OD_LKEEPALIVE,
	OD_LWORKERS,
	OD_LCLIENT_MAX,
	OD_LSERVER,
	OD_LROUTING,
	OD_LDEFAULT,
	OD_LROUTE,
	OD_LMODE,
	OD_LDATABASE,
	OD_LUSER,
	OD_LPASSWORD,
	OD_LTTL,
	OD_LPOOL_MIN,
	OD_LPOOL_MAX,
};

typedef struct {
	od_lex_t lex;
	od_log_t *log;
	od_scheme_t *scheme;
} od_config_t;

void od_configinit(od_config_t*, od_log_t*, od_scheme_t*);
int  od_configopen(od_config_t*, char*);
void od_configclose(od_config_t*);
int  od_configparse(od_config_t*);

#endif

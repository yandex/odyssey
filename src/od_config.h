#ifndef OD_CONFIG_H
#define OD_CONFIG_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

enum
{
	OD_LYES = OD_LCUSTOM,
	OD_LNO,
	OD_LON,
	OD_LOFF,
	OD_LDAEMONIZE,
	OD_LLOG_DEBUG,
	OD_LLOG_CONFIG,
	OD_LLOG_FILE,
	OD_LPID_FILE,
	OD_LSYSLOG,
	OD_LSYSLOG_IDENT,
	OD_LSYSLOG_FACILITY,
	OD_LSTATS_PERIOD,
	OD_LPOOLING,
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
	OD_LTLS_MODE,
	OD_LTLS_CA_FILE,
	OD_LTLS_KEY_FILE,
	OD_LTLS_CERT_FILE,
	OD_LTLS_PROTOCOLS,
	OD_LSTORAGE,
	OD_LROUTE,
	OD_LDEFAULT,
	OD_LMODE,
	OD_LDATABASE,
	OD_LUSER,
	OD_LPASSWORD,
	OD_LCANCEL,
	OD_LDISCARD,
	OD_LROLLBACK,
	OD_LPOOL_SIZE,
	OD_LPOOL_TIMEOUT,
	OD_LPOOL_TTL,
	OD_LAUTHENTICATION,
	OD_LDENY
};

typedef struct
{
	od_lex_t lex;
	od_log_t *log;
	od_scheme_t *scheme;
} od_config_t;

void od_config_init(od_config_t*, od_log_t*, od_scheme_t*);
int  od_config_open(od_config_t*, char*);
void od_config_close(od_config_t*);
int  od_config_parse(od_config_t*);

#endif /* OD_CONFIG_H */

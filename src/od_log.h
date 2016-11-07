#ifndef OD_LOG_H_
#define OD_LOG_H_

/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

typedef struct odlog_t odlog_t;

struct odlog_t {
	int pid;
	int fd;
};

int od_loginit(odlog_t*);
int od_logopen(odlog_t*, char*);
int od_logclose(odlog_t*);
int od_log(odlog_t*, char*, ...);

#endif

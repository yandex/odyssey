#ifndef OD_H_
#define OD_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_t od_t;

struct od_t {
	odpid_t    pid;
	odlog_t    log;
	odconfig_t config;
	odscheme_t scheme;
};

void od_init(od_t*);
void od_free(od_t*);
int  od_main(od_t*, int, char**);

#endif

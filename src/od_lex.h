#ifndef OD_LEX_H_
#define OD_LEX_H_

/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

typedef struct odkeyword_t odkeyword_t;
typedef struct odtoken_t odtoken_t;
typedef struct odlex_t odlex_t;

enum {
	OD_LERROR  = -1,
	OD_LEOF    =  0,
	OD_LNUMBER =  1000,
	OD_LPUNCT,
	OD_LID,
	OD_LSTRING,
	OD_LCUSTOM
};

struct odkeyword_t {
	char *name;
	int   size;
	int   id;
};

struct odtoken_t {
	int id;
	union {
		uint64_t num;
		char *string;
	} v;
	int line;
	odlist_t link_alloc;
	odlist_t link;
};

struct odlex_t {
	char        *buf;
	int          size;
	int          pos;
	int          line;
	odkeyword_t *keywords;
	int          count;
	odlist_t     stack;
	odlist_t     list;
	char        *error;
};

void  od_lexinit(odlex_t*, odkeyword_t*, char*, int);
void  od_lexfree(odlex_t*);
char *od_lexname_of(odlex_t*, int);
void  od_lexpush(odlex_t*, odtoken_t*);
int   od_lexpop(odlex_t*, odtoken_t**);

#endif

#ifndef OD_LEX_H_
#define OD_LEX_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_keyword_t od_keyword_t;
typedef struct od_token_t od_token_t;
typedef struct od_lex_t od_lex_t;

enum {
	OD_LERROR  = -1,
	OD_LEOF    =  0,
	OD_LNUMBER =  1000,
	OD_LPUNCT,
	OD_LID,
	OD_LSTRING,
	OD_LCUSTOM
};

struct od_keyword_t {
	char *name;
	int   size;
	int   id;
};

struct od_token_t {
	int id;
	union {
		uint64_t num;
		char *string;
	} v;
	int line;
	odlist_t link_alloc;
	odlist_t link;
};

struct od_lex_t {
	char         *buf;
	int           size;
	int           pos;
	int           line;
	od_keyword_t *keywords;
	int           count;
	odlist_t      stack;
	odlist_t      list;
	char         *error;
};

void  od_lexinit(od_lex_t*);
void  od_lexopen(od_lex_t*, od_keyword_t*, char*, int);
void  od_lexfree(od_lex_t*);
char *od_lexname_of(od_lex_t*, int);
void  od_lexpush(od_lex_t*, od_token_t*);
int   od_lexpop(od_lex_t*, od_token_t**);

#endif

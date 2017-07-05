#ifndef OD_LEX_H
#define OD_LEX_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_keyword od_keyword_t;
typedef struct od_token   od_token_t;
typedef struct od_lex     od_lex_t;

enum
{
	OD_LERROR  = -1,
	OD_LEOF    =  0,
	OD_LNUMBER =  1000,
	OD_LPUNCT,
	OD_LID,
	OD_LSTRING,
	OD_LCUSTOM
};

struct od_keyword
{
	char *name;
	int   size;
	int   id;
};

struct od_token
{
	int id;
	union {
		uint64_t num;
		char *string;
	} v;
	int line;
	od_list_t link_alloc;
	od_list_t link;
};

struct od_lex
{
	char         *buf;
	int           size;
	int           pos;
	int           line;
	od_keyword_t *keywords;
	int           count;
	od_list_t     stack;
	od_list_t     list;
	char         *error;
};

void  od_lex_init(od_lex_t*);
void  od_lex_open(od_lex_t*, od_keyword_t*, char*, int);
void  od_lex_free(od_lex_t*);
char *od_lex_name_of(od_lex_t*, int);
void  od_lex_push(od_lex_t*, od_token_t*);
int   od_lex_pop(od_lex_t*, od_token_t**);

#endif /* OD_LEX_H */

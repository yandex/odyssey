
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_lex.h"

void od_lexinit(od_lex_t *l)
{
	memset(l, 0, sizeof(*l));
	od_listinit(&l->stack);
	od_listinit(&l->list);
}

void od_lexopen(od_lex_t *l, od_keyword_t *list, char *buf, int size)
{
	l->buf      = buf;
	l->size     = size;
	l->keywords = list;
}

void od_lexfree(od_lex_t *l)
{
	odlist_t *i, *n;
	od_listforeach_safe(&l->list, i, n) {
		od_token_t *tk = od_container_of(i, od_token_t, link_alloc);
		if (tk->id == OD_LSTRING ||
		    tk->id == OD_LID) {
			free(tk->v.string);
		}
		free(tk);
	}
	if (l->buf)
		free(l->buf);
	if (l->error)
		free(l->error);
}

char *od_lexname_of(od_lex_t *l, int id)
{
	switch (id) {
	case OD_LEOF:    return "eof";
	case OD_LERROR:  return "error";
	case OD_LNUMBER: return "number";
	case OD_LSTRING: return "string";
	case OD_LID:     return "name";
	case OD_LPUNCT:  return "punctuation";
	}
	int i;
	for (i = 0 ; l->keywords[i].name ; i++)
		if (l->keywords[i].id == id)
			return l->keywords[i].name;
	return NULL;
}

void od_lexpush(od_lex_t *l, od_token_t *tk)
{
	od_listpush(&l->stack, &tk->link);
	l->count++;
}

static int
od_lexerror(od_lex_t *l, const char *fmt, ...)
{
	if (fmt == NULL)
		return OD_LEOF;
	if (l->error)
		free(l->error);
	char msg[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);
	l->error = strdup(msg);
	return OD_LERROR;
}

static inline od_token_t*
od_lexalloc(od_lex_t *l, int id, int line)
{
	od_token_t *tk = malloc(sizeof(od_token_t));
	if (tk == NULL)
		return NULL;
	memset(tk, 0, sizeof(*tk));
	tk->id   = id;
	tk->line = line;
	od_listinit(&tk->link);
	od_listinit(&tk->link_alloc);
	od_listappend(&l->list, &tk->link_alloc);
	return tk;
}

static inline int
od_lexnext(od_lex_t *l) {
	if (l->pos == l->size)
		return 0;
	l->pos++;
	return 1;
}

static inline uint8_t
od_lexchar(od_lex_t *l) {
	return *(l->buf + l->pos);
}

static inline od_token_t*
od_lexpop_stack(od_lex_t *l)
{
	if (l->count == 0)
		return NULL;
	od_token_t *tk = od_container_of(l->stack.next, od_token_t, link);
	od_listunlink(&tk->link);
	l->count--;
	return tk;
}

int od_lexpop(od_lex_t *l, od_token_t **result)
{
	/* stack first */
	if (l->count) {
		*result = od_lexpop_stack(l);
		if ((*result)->id == OD_LPUNCT)
			return (*result)->v.num;
		return (*result)->id;
	}

	/* skip white-spaces and comments */
	unsigned char ch;
	while (1) {
		if (l->pos == l->size) {
			*result = od_lexalloc(l, OD_LEOF, l->line);
			if (*result == NULL)
				return od_lexerror(l, "memory allocation error");
			return OD_LEOF;
		}
		ch = od_lexchar(l);
		if (isspace(ch)) {
			if (ch == '\n') {
				if (((l->pos + 1) != l->size))
					l->line++;
			}
			od_lexnext(l);
			continue;
		}
		if (ch == '#') {
			while (1) {
				if (l->pos == l->size) {
					*result = od_lexalloc(l, OD_LEOF, l->line);
					if (*result == NULL)
						return od_lexerror(l, "memory allocation error");
					return OD_LEOF;
				}
				od_lexnext(l);
				ch = od_lexchar(l);
				if (ch == '\n') {
					if (((l->pos + 1) != l->size))
						l->line++;
					od_lexnext(l);
					break;
				}
			}
			continue;
		}
		break;
	}

	/* token position */
	int line  = l->line;
	int start = l->pos;
	int size  = 0;

	/* string */
	ch = od_lexchar(l);
	if (ch == '\"') {
		start++;
		while (1) {
			if (od_lexnext(l) == 0)
				return od_lexerror(l, "bad string definition");
			ch = od_lexchar(l);
			if (ch == '\"')
				break;
			if (ch == '\n')
				return od_lexerror(l, "bad string definition");
		}
		size = l->pos - start;
		od_lexnext(l);
		*result = od_lexalloc(l, OD_LSTRING, line);
		if (*result == NULL)
			return od_lexerror(l, "memory allocation error");
		od_token_t *tk = *result;
		tk->v.string = malloc(size + 1);
		if (tk->v.string == NULL)
			return od_lexerror(l, "memory allocation error");
		memcpy(tk->v.string, l->buf + start, size);
		tk->v.string[size] = 0;
		return OD_LSTRING;
	}

	/* punctuation */
	if (ispunct(ch) && ch != '_') {
		od_lexnext(l);
		*result = od_lexalloc(l, OD_LPUNCT, line);
		if (*result == NULL)
			return od_lexerror(l, "memory allocation error");
		(*result)->v.num = ch;
		return ch;
	}

	/* numeric value */
	if (isdigit(ch)) {
		int64_t num = 0;
		while (1) {
			ch = od_lexchar(l);
			if (isdigit(ch))
				num = (num * 10) + ch - '0';
			else
				break;
			if (od_lexnext(l) == 0)
				break;
		}
		*result = od_lexalloc(l, OD_LNUMBER, line);
		if (*result == NULL)
			return od_lexerror(l, "memory allocation error");
		(*result)->v.num = num;
		return OD_LNUMBER;
	}

	/* skip to the end of lexem */
	while (1) {
		ch = od_lexchar(l);
		if (isspace(ch) || (ispunct(ch) && ch != '_'))
			break;
		if (od_lexnext(l) == 0)
			break;
	}
	size = l->pos - start;

	/* match keyword */
	int i = 0;
	for ( ; l->keywords[i].name ; i++) {
		if (l->keywords[i].size != size)
			continue;
		if (strncasecmp(l->keywords[i].name, l->buf + start, size) == 0) {
			*result = od_lexalloc(l, l->keywords[i].id, line);
			if (*result == NULL)
				return od_lexerror(l, "memory allocation error");
			return l->keywords[i].id;
		}
	}

	/* identification */
	*result = od_lexalloc(l, OD_LID, line);
	if (*result == NULL)
		return od_lexerror(l, "memory allocation error");
	od_token_t *tk = *result;
	tk->v.string = malloc(size + 1);
	if (tk->v.string == NULL)
		return od_lexerror(l, "memory allocation error");
	memcpy(tk->v.string, l->buf + start, size);
	tk->v.string[size] = 0;
	return OD_LID;
}

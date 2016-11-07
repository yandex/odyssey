
/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_scheme.h"

void od_schemeinit(odscheme_t *scheme)
{
	(void)scheme;
}

void od_schemefree(odscheme_t *scheme)
{
	(void)scheme;
}

odscheme_server_t*
od_scheme_addserver(odscheme_t *scheme)
{
	odscheme_server_t *s =
		(odscheme_server_t*)malloc(sizeof(*s));
	if (s == NULL)
		return NULL;
	memset(s, 0, sizeof(*s));
	od_listinit(&s->link);
	od_listappend(&scheme->servers, &s->link);
	return s;
}

odscheme_route_t*
od_scheme_addroute(odscheme_t *scheme)
{
	odscheme_route_t *r =
		(odscheme_route_t*)malloc(sizeof(*r));
	if (r == NULL)
		return NULL;
	memset(r, 0, sizeof(*r));
	od_listinit(&r->link);
	od_listappend(&scheme->routing_table, &r->link);
	return r;
}

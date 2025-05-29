/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

od_keyword_t od_virtual_process_keywords[] = {
	od_keyword("odyssey", OD_VIRTUAL_LODYSSEY),
	od_keyword("target_session_attrs", OD_VIRTUAL_LTSA),
	od_keyword("set", OD_VIRTUAL_LSET),
	od_keyword("show", OD_VIRTUAL_LSHOW),
	{ 0, 0, 0 }
};
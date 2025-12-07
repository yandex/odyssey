/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <query_processing.h>

od_keyword_t od_query_process_keywords[] = {
	od_keyword("odyssey", OD_QUERY_PROCESSING_LODYSSEY),
	od_keyword("target_session_attrs", OD_QUERY_PROCESSING_LTSA),
	od_keyword("application_name", OD_QUERY_PROCESSING_LAPPNAME),
	od_keyword("set", OD_QUERY_PROCESSING_LSET),
	od_keyword("to", OD_QUERY_PROCESSING_LTO),
	od_keyword("show", OD_QUERY_PROCESSING_LSHOW),
	{ 0, 0, 0 }
};

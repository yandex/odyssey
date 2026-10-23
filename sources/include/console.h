#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <tuple_table_slot.h>

#include <types.h>

int od_console_query(od_client_t *, od_tuple_table_slot_t *, char *, uint32_t);
int od_console_fill_global_pstmt_rows(od_client_t *, od_tuple_table_slot_t *);

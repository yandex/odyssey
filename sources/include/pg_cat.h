#pragma once

/*
 * pg_cat.
 *
 * PostgreSQL system catalog binary constants (pg_type.h OIDs).
 */

#define PG_BOOLOID 16
#define PG_INT8OID 20
#define PG_TEXTOID 25
#define PG_INT4OID 23
#define PG_FLOAT4OID 700

/* typlen: -1 for varlena types */
#define PG_BOOL_LEN 1
#define PG_INT8_LEN 8
#define PG_TEXT_LEN -1
#define PG_INT4_LEN 4
#define PG_FLOAT4_LEN 4

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

typedef enum {
	OD_TARGET_SESSION_ATTRS_RW,
	OD_TARGET_SESSION_ATTRS_RO,
	OD_TARGET_SESSION_ATTRS_ANY,
	OD_TARGET_SESSION_ATTRS_UNDEF,
} od_target_session_attrs_t;

static inline char *
od_target_session_attrs_to_str(od_target_session_attrs_t tsa)
{
	switch (tsa) {
	case OD_TARGET_SESSION_ATTRS_RW:
		return "read-write";
	case OD_TARGET_SESSION_ATTRS_RO:
		return "read-only";
	case OD_TARGET_SESSION_ATTRS_ANY:
		return "any";
	case OD_TARGET_SESSION_ATTRS_UNDEF:
		return "no specifed";
	}

	return "<unknown>";
}

static inline char *
od_target_session_attrs_to_pg_mode_str(od_target_session_attrs_t tsa)
{
	switch (tsa) {
	case OD_TARGET_SESSION_ATTRS_RW:
		return "primary";
	case OD_TARGET_SESSION_ATTRS_RO:
		return "standby";
	case OD_TARGET_SESSION_ATTRS_ANY:
		return "any";
	case OD_TARGET_SESSION_ATTRS_UNDEF:
		return "no specifed";
	}

	return "<unknown>";
}

od_target_session_attrs_t od_tsa_get_effective(od_client_t *client);

int od_tsa_match_rw_state(od_target_session_attrs_t attrs, int is_rw);

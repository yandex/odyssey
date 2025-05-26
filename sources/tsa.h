#ifndef ODYSSEY_TARGET_SESSION_ATTRS_H
#define ODYSSEY_TARGET_SESSION_ATTRS_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef enum {
	OD_TARGET_SESSION_ATTRS_RW,
	OD_TARGET_SESSION_ATTRS_RO,
	OD_TARGET_SESSION_ATTRS_ANY,
	OD_TARGET_SESSION_ATTRS_UNDEF,
} od_target_session_attrs_t;

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

#endif /* ODYSSEY_RULE_STORAGE_H */
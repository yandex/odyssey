/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

od_target_session_attrs_t od_tsa_get_effective(od_client_t *client)
{
	od_route_t *route = client->route;

	od_target_session_attrs_t default_tsa =
		client->config_listen->target_session_attrs;
	od_target_session_attrs_t effective_tsa = OD_TARGET_SESSION_ATTRS_UNDEF;

	if (route->rule->target_session_attrs !=
	    OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = route->rule->target_session_attrs;
	}

	/* if listen config specifies tsa, it is considered as more powerfull default */
	if (default_tsa != OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = default_tsa;
	}

	kiwi_var_t *hint_var = kiwi_vars_get(
		&client->vars, KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS);

	/* XXX: TODO refactor kiwi_vars_get to avoid strcmp */
	if (hint_var != NULL) {
		if (strncmp(hint_var->value, "read-only",
			    hint_var->value_len) == 0) {
			effective_tsa = OD_TARGET_SESSION_ATTRS_RO;
		}
		if (strncmp(hint_var->value, "read-write",
			    hint_var->value_len) == 0) {
			effective_tsa = OD_TARGET_SESSION_ATTRS_RW;
		}

		if (strncmp(hint_var->value, "any", hint_var->value_len) == 0) {
			effective_tsa = OD_TARGET_SESSION_ATTRS_ANY;
		}
	}

	/* 'read-write' and 'read-only' is passed as is, 'any' or unknown == any */

	if (effective_tsa == OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = OD_TARGET_SESSION_ATTRS_ANY;
	}

	return effective_tsa;
}

int od_tsa_match_rw_state(od_target_session_attrs_t attrs, int is_rw)
{
	switch (attrs) {
	case OD_TARGET_SESSION_ATTRS_ANY:
		return 1;
	case OD_TARGET_SESSION_ATTRS_RW:
		return is_rw;
	case OD_TARGET_SESSION_ATTRS_RO:
		/* this is primary, but we are forced to find ro backend */
		return !is_rw;
	case OD_TARGET_SESSION_ATTRS_UNDEF:
		return 0;
	default:
		abort();
	}
}

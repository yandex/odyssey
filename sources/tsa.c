/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <tsa.h>
#include <client.h>
#include <route.h>
#include <rules.h>
#include <config.h>
#include <system.h>

/*
 * parse the odyssey.target_session_attrs value; the length is checked
 * first so the comparisons stay bounded. Case-insensitive, like the
 * SET-processing it replaces
 */
static int tsa_value_eq(const char *value, const char *cstr, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		char c = value[i];
		if (c >= 'A' && c <= 'Z') {
			c = (char)(c - 'A' + 'a');
		}
		if (c != cstr[i]) {
			return 0;
		}
	}
	/* the caller only passes matching literal lengths */
	return cstr[len] == '\0';
}

od_target_session_attrs_t od_target_session_attrs_parse(const char *value,
							size_t value_len)
{
	switch (value_len) {
	case 3:
		if (tsa_value_eq(value, "any", 3)) {
			return OD_TARGET_SESSION_ATTRS_ANY;
		}
		break;
	case 9:
		if (tsa_value_eq(value, "read-only", 9)) {
			return OD_TARGET_SESSION_ATTRS_RO;
		}
		break;
	case 10:
		if (tsa_value_eq(value, "read-write", 10)) {
			return OD_TARGET_SESSION_ATTRS_RW;
		}
		break;
	case 14:
		if (tsa_value_eq(value, "prefer-standby", 14)) {
			return OD_TARGET_SESSION_ATTRS_PREFER_STANDBY;
		}
		break;
	default:
		break;
	}

	return OD_TARGET_SESSION_ATTRS_UNDEF;
}

od_target_session_attrs_t od_tsa_get_effective(od_client_t *client)
{
	od_route_t *route = client->route;

	od_target_session_attrs_t default_tsa = OD_TARGET_SESSION_ATTRS_UNDEF;
	if (client->source != NULL) {
		default_tsa = client->source->config->target_session_attrs;
	}
	od_target_session_attrs_t effective_tsa = OD_TARGET_SESSION_ATTRS_UNDEF;

	if (route->rule->target_session_attrs !=
	    OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = route->rule->target_session_attrs;
	}

	/* if listen config specifies tsa, it is considered as more powerfull default */
	if (default_tsa != OD_TARGET_SESSION_ATTRS_UNDEF) {
		effective_tsa = default_tsa;
	}

	/*
	 * the SET odyssey.target_session_attrs hint is parsed once, on the
	 * virtual SET, and kept as an enum on the client
	 */
	if (client->tsa_hint_set) {
		switch (client->tsa_hint) {
		case OD_TARGET_SESSION_ATTRS_RO:
		case OD_TARGET_SESSION_ATTRS_RW:
		case OD_TARGET_SESSION_ATTRS_PREFER_STANDBY:
		case OD_TARGET_SESSION_ATTRS_ANY:
			effective_tsa = client->tsa_hint;
			break;
		case OD_TARGET_SESSION_ATTRS_UNDEF:
			/* unrecognized value: keep the listen/rule default */
			break;
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
	case OD_TARGET_SESSION_ATTRS_PREFER_STANDBY:
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

int od_tsa_auto_route_ro_enabled(od_client_t *client)
{
	int by_rule = client->rule->auto_route_ro_on_standby;
	int by_listen = 0;

	od_config_listen_t *listencfg = client->source->config;
	if (listencfg != NULL) {
		by_listen = listencfg->auto_route_ro_on_standby;
	}

	return by_rule || by_listen;
}

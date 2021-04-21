#ifndef ODYSSEY_ADDON_H
#define ODYSSEY_ADDON_H

typedef struct od_extention od_extention_t;

struct od_extention {
	od_module_t *modules;
#ifdef LDAP_FOUND
	od_ldap_endpoint_t *ldaps;
#endif

	od_global_t *glob;
};

static inline od_retcode_t od_extentions_init(od_extention_t *extentions)
{
	extentions->modules = malloc(sizeof(od_module_t));
	od_modules_init(extentions->modules);
#ifdef LDAP_FOUND
	extentions->ldaps = od_ldap_endpoint_alloc();
	//od_ldap_endpoint_init(extentions->ldaps);
#endif

	return OK_RESPONSE;
}

static inline od_retcode_t od_extention_free(od_logger_t *l,
					     od_extention_t *extentions)
{
	if (extentions->modules) {
		od_modules_unload(l, extentions->modules);
	}

#ifdef LDAP_FOUND
	if (extentions->ldaps) {
		// TODO : unload alll & free
	}
#endif

	return OK_RESPONSE;
}

#endif /* ODYSSEY_ADDON_H */

#ifndef ODYSSEY_ADDON_H
#define ODYSSEY_ADDON_H

typedef struct od_extension od_extension_t;

struct od_extension {
	od_module_t *modules;

	od_global_t *glob;
};

static inline od_retcode_t od_extensions_init(od_extension_t *extensions)
{
	extensions->modules = malloc(sizeof(od_module_t));
	if (extensions->modules == NULL) {
		return 1;
	}

	od_modules_init(extensions->modules);

	return OK_RESPONSE;
}

static inline od_retcode_t od_extension_free(od_logger_t *l,
					     od_extension_t *extensions)
{
	if (extensions->modules) {
		od_modules_unload(l, extensions->modules);
	}

	free(extensions->modules);
	extensions->modules = NULL;

	return OK_RESPONSE;
}

#endif /* ODYSSEY_ADDON_H */

#pragma once

struct od_extension {
	od_module_t *modules;

	od_global_t *glob;
};

static inline od_retcode_t od_extensions_init(od_extension_t *extensions)
{
	extensions->modules = od_malloc(sizeof(od_module_t));
	if (extensions->modules == NULL) {
		return 1;
	}

	od_modules_init(extensions->modules);

	return OK_RESPONSE;
}

static inline od_extension_t *od_extensions_create()
{
	od_extension_t *e = od_malloc(sizeof(od_extension_t));
	if (e == NULL) {
		return NULL;
	}

	if (od_extensions_init(e)) {
		od_free(e);
		return NULL;
	}

	return e;
}

static inline od_retcode_t od_extension_free(od_logger_t *l,
					     od_extension_t *extensions)
{
	if (extensions == NULL) {
		return OK_RESPONSE;
	}

	if (extensions->modules) {
		od_modules_unload(l, extensions->modules);
	}

	od_free(extensions->modules);
	extensions->modules = NULL;

	od_free(extensions);

	return OK_RESPONSE;
}

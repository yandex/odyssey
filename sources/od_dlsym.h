#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "dlfcn.h"

#define od_dlopen(path) dlopen((char *)path, RTLD_NOW | RTLD_GLOBAL)
#define od_dlsym(handle, symbol) dlsym(handle, symbol)
#define od_dlerror dlerror
#define od_dlclose(handle) dlclose(handle)

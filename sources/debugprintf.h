#ifndef ODYSSEY_DEBUGPRINTF_H
#define ODYSSEY_DEBUGPRINTF_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "macro.h"
#include "util.h"

void
od_dbg_printf(char *fmt, ...);

#ifndef OD_DEVEL_LVL
/* set "release" mode by default */
#define OD_DEVEL_LVL -1
#endif

#define od_dbg_printf_on_dvl_lvl(debug_lvl, fmt, ...)                          \
	if (OD_DEVEL_LVL >= debug_lvl) {                                           \
		od_dbg_printf(fmt, __VA_ARGS__);                                       \
	}

#endif /* ODYSSEY_DEBUGPRINTF_H */

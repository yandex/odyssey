#ifndef ODYSSEY_MODULE_H
#define ODYSSEY_MODULE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "list.h"
#include <kiwi.h>
#include "id.h"
#include "pid.h"
#include "logger.h"
#include "status.h"
#include "readahead.h"
#include "io.h"
#include "relay.h"

#include "server.h"
#include "rules.h"
#include "client.h"

#include "config_common.h"

#define OD_LOAD_MODULE         "od_module"
#define od_load_module(handle) (od_module_t *)od_dlsym(handle, OD_LOAD_MODULE)

#define OD_MODULE_CB_OK_RETCODE   0
#define OD_MODULE_CB_FAIL_RETCODE -1

typedef int (*module_init_cb)();
typedef int (*client_auth_attempt_cb)(od_client_t *c);
typedef int (*client_auth_complete_cb)(od_client_t *c, int rc);
typedef int (*client_disconnect_cb)(od_client_t *c, od_frontend_status_t s);
typedef int (*config_custom_init_cb)(char *user_name,
                                     od_config_reader_t *cr,
                                     od_token_t *token);
typedef int (*module_unload_cb)(void);

typedef module_init_cb module_init_cb_t;
typedef client_auth_attempt_cb client_auth_attempt_cb_t;
typedef client_auth_complete_cb client_auth_complete_cb_t;
typedef client_disconnect_cb client_disconnect_cb_t;
typedef config_custom_init_cb config_custom_init_cb_t;
typedef module_unload_cb module_unload_cb_t;

#define MAX_MODULE_PATH_LEN 2048

struct od_module
{
	void *handle;
	char path[MAX_MODULE_PATH_LEN];

	/*       Handlers                */
	/*---------------------------------*/
	module_init_cb_t module_init_cb;
	client_auth_attempt_cb_t auth_attempt_cb;
	client_auth_complete_cb_t auth_complete_cb;
	client_disconnect_cb_t disconnect_cb;
	config_custom_init_cb_t config_init_cb;
	module_unload_cb_t unload_cb;

	/*---------------------------------*/
	od_list_t link;
};

typedef struct od_module od_module_t;

void
od_modules_init(od_module_t *module);

int
od_target_module_add(od_logger_t *logger,
                     od_module_t *modules,
                     char *target_module_path);
int
od_target_module_unload(od_logger_t *logger,
                        od_module_t *modules,
                        char *target_module);
int
od_modules_unload(od_logger_t *logger, od_module_t *modules);
// function tio perform "fast" unload all modules,
// here we do not wait for module-defined unload callback
int
od_modules_unload_fast(od_module_t *modules);

#endif /* ODYSSEY_MODULE_H */

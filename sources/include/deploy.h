#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>
#include <common_const.h>

/* The pending BEGIN failed and its error was returned to the client. */
#define OD_DEPLOY_BEGIN_ERROR 1

int od_deploy(od_client_t *);

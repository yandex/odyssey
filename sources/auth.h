#ifndef ODYSSEY_AUTH_H
#define ODYSSEY_AUTH_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

int
od_auth_frontend(od_client_t *);
int
od_auth_backend(od_server_t *, machine_msg_t *);

#endif /* ODYSSEY_AUTH_H */

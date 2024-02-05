#ifndef ODYSSEY_IAMPROXY_H
#define ODYSSEY_IAMPROXY_H

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>


int mdb_iamproxy_authenticate_user(char *username, char *token,
				   od_instance_t *instance,
				   od_client_t *client);

#endif /* ODYSSEY_IAMPROXy_H */

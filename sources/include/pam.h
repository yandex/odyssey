#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <list.h>

struct od_pam_auth_data {
	int msg_style;
	char *value;
	od_list_t link;
};

typedef struct od_pam_auth_data od_pam_auth_data_t;

int od_pam_auth(char *od_pam_service, char *usrname,
		od_pam_auth_data_t *auth_data, machine_io_t *io);

void od_pam_convert_passwd(od_pam_auth_data_t *d, char *passwd);

od_pam_auth_data_t *od_pam_auth_data_create(void);

void od_pam_auth_data_free(od_pam_auth_data_t *d);

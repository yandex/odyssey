
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>

#include <kiwi.h>
#include <odyssey.h>

static int
od_pam_conversation(int msgc,
                    const struct pam_message **msgv,
                    struct pam_response **rspv,
                    void *authdata)
{
	char *password = (char *)authdata;
	if (msgc < 1 || msgv == NULL || password == NULL)
		return PAM_CONV_ERR;

	*rspv = malloc(msgc * sizeof(struct pam_response));
	if (*rspv == NULL)
		return PAM_CONV_ERR;
	memset(*rspv, 0, msgc * sizeof(struct pam_response));

	int rc      = PAM_SUCCESS;
	int counter = 0;
	for (; counter < msgc; counter++) {
		if (msgv[counter]->msg_style == PAM_PROMPT_ECHO_OFF) {
			(*rspv)[counter].resp = strdup(password);
			if ((*rspv)[counter].resp == NULL) {
				rc = PAM_CONV_ERR;
				break;
			}
		}
	}

	if (rc != PAM_SUCCESS) {
		for (; counter >= 0; counter--) {
			if (msgv[counter]->msg_style == PAM_PROMPT_ECHO_OFF)
				free((*rspv)[counter].resp);
		}
		free(*rspv);
		*rspv = NULL;
	}

	return rc;
}

int
od_pam_auth(char *od_pam_service,
            od_pam_auth_data_t *auth_data,
            machine_io_t *io)
{
	struct pam_conv conv = {
		od_pam_conversation,
		.appdata_ptr = NULL,
	};
	char *usrname;

	od_list_t *i;
	od_list_foreach(&auth_data->link, i)
	{
		od_pam_auth_data_t *param;
		param = od_container_of(i, od_pam_auth_data_t, link);
		switch (param->key) {
			case PAM_USER:
				usrname = param->value;
				break;
			case PAM_AUTHTOK:
				conv.appdata_ptr = param->value;
				break;
			default:
				break;
		}
	}

	pam_handle_t *pamh = NULL;
	int rc;
	rc = pam_start(od_pam_service, usrname, &conv, &pamh);
	if (rc != PAM_SUCCESS)
		goto error;

	char peer[128];
	od_getpeername(io, peer, sizeof(peer), 1, 0);
	rc = pam_set_item(pamh, PAM_RHOST, peer);
	if (rc != PAM_SUCCESS) {
		goto error;
	}

	rc = pam_authenticate(pamh, 0);
	if (rc != PAM_SUCCESS)
		goto error;

	rc = pam_acct_mgmt(pamh, PAM_SILENT);
	if (rc != PAM_SUCCESS)
		goto error;

	rc = pam_end(pamh, rc);
	if (rc != PAM_SUCCESS)
		return -1;

	return 0;

error:
	pam_end(pamh, rc);
	return -1;
}

void
od_pam_convert_usr_passwd(od_pam_auth_data_t *d, char *usr, char *passwd)
{
	od_pam_auth_data_t usr_data;
	usr_data.key   = PAM_USER;
	usr_data.value = usr;

	od_list_init(&usr_data.link);

	od_pam_auth_data_t passwd_data;
	passwd_data.key   = PAM_AUTHTOK;
	passwd_data.value = passwd;

	od_list_append(&d->link, &passwd_data.link);
	od_list_append(&d->link, &usr_data.link);

	return;
}

od_pam_auth_data_t *
od_pam_auth_data_create()
{
	od_pam_auth_data_t *d;
	d = (od_pam_auth_data_t *)malloc(sizeof(*d));
	if (d == NULL)
		return NULL;
	od_list_init(&d->link);

	return d;
}

void
od_pam_auth_data_free(od_pam_auth_data_t *d)
{
	od_list_unlink(&d->link);
	free(d);
}
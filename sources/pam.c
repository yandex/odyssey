
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

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
od_pam_auth(char *od_pam_service, kiwi_var_t *user, kiwi_password_t *password, machine_io_t *io)
{
	struct pam_conv conv = {
		od_pam_conversation,
		.appdata_ptr = (void *)password->password,
	};

	pam_handle_t *pamh = NULL;
	int rc;
	rc = pam_start(od_pam_service, user->value, &conv, &pamh);
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

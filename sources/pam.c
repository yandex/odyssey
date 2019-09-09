/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <security/pam_appl.h>
#include <kiwi.h>

static int
pam_conversation(int msgc, const struct pam_message **msgv, struct pam_response **rspv, void *authdata) {
	char *password = (char *) authdata;
	int counter, retcode;
	
	if (msgc < 1 || msgv == NULL || password == NULL) {
		return PAM_CONV_ERR;
	}
	*rspv = malloc(msgc * sizeof(struct pam_response));
	if (*rspv == NULL) {
		return PAM_CONV_ERR;
	}
	
	memset(*rspv, 0, msgc * sizeof(struct pam_response));
	retcode = PAM_SUCCESS;
	
	for (counter = 0; counter < msgc; counter++) {
		if (retcode != PAM_SUCCESS) {
			break;
		}
		if (msgv[counter]->msg_style == PAM_PROMPT_ECHO_OFF) {
			(*rspv)[counter].resp = strdup(password);
			if ((*rspv)[counter].resp == NULL) {
				retcode = PAM_CONV_ERR;
			}
		}
	}
	
	if (retcode != PAM_SUCCESS) {
		for (counter = 0; counter < msgc; counter++)
			free((*rspv)[counter].resp);
		free(*rspv);
	}
	return retcode;
}

int
od_check_pam_auth(char *od_pam_service, 
		kiwi_var_t *user, kiwi_password_t *password) {
	struct pam_conv conv = {
		pam_conversation,
		.appdata_ptr = (void *)password->password,
	};

	/* pam handler */
	pam_handle_t *pamh = NULL;
	int retval;
	
	/* init PAM transaction */
	retval = pam_start(od_pam_service, user->value, &conv, &pamh);
	if (retval != PAM_SUCCESS) {
		pam_end(pamh, retval);
		return -1;
	}
	
	retval = pam_authenticate(pamh, 0);
	if (retval != PAM_SUCCESS) {
		pam_end(pamh, retval);
		return -1;
	}
	
	retval = pam_acct_mgmt(pamh, PAM_SILENT);
	if (retval != PAM_SUCCESS) {
		pam_end(pamh, retval);
		return -1;
	}

	return pam_end(pamh, retval) == PAM_SUCCESS ? 0 : 1;
}

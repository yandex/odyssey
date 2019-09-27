#ifndef ODYSSEY_PAM_H
#define ODYSSEY_PAM_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

int
od_pam_auth(char *od_pam_service, kiwi_var_t *user, kiwi_password_t *password);

#endif /* ODYSSEY_PAM_H */

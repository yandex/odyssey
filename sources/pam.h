#ifndef ODYSSEY_PAM_H
#define ODYSSEY_PAM_H

#include <kiwi.h>

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/
extern int
od_check_pam_auth(char *od_pam_service, kiwi_var_t *user, kiwi_password_t *password);

#endif /* ODYSSEY_PAM_H */

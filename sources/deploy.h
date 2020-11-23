#ifndef ODYSSEY_DEPLOY_H
#define ODYSSEY_DEPLOY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#define OD_QRY_MAX_SZ 512 /* odyssey maximum allowed query size */

int
od_deploy(od_client_t *, char *);

#endif /* ODYSSEY_DEPLOY_H */

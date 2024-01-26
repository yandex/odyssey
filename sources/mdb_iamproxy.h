#ifndef ODYSSEY_IAMPROXY_H
#define ODYSSEY_IAMPROXY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int mdb_iamproxy_recv_from_socket(int socket_fd, char *msg_body);
int mdb_iamproxy_send_to_socket(int socket_fd, const char *send_msg);
int mdb_iamproxy_authenticate_user(const char *username, const char *token, od_instance_t *instance, od_client_t *client);

#endif /* ODYSSEY_IAMPROXy_H */

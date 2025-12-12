#ifndef OD_SYSTEMD_NOTIFY_H
#define OD_SYSTEMD_NOTIFY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

void od_systemd_notify_ready(void);
void od_systemd_notify_reloading(void);
void od_systemd_notify_stopping(void);
void od_systemd_notify_mainpid(pid_t pid);

#endif /* OD_SYSTEMD_NOTIFY_H */

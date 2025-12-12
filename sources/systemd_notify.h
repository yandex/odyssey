#ifndef OD_SYSTEMD_NOTIFY_H
#define OD_SYSTEMD_NOTIFY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#ifdef HAVE_SYSTEMD

void od_systemd_notify_ready(void);
void od_systemd_notify_reloading(void);
void od_systemd_notify_stopping(void);
void od_systemd_notify_mainpid(pid_t pid);

#else

/* No-op stubs when systemd is not available */
static inline void od_systemd_notify_ready(void)
{
}
static inline void od_systemd_notify_reloading(void)
{
}
static inline void od_systemd_notify_stopping(void)
{
}
static inline void od_systemd_notify_mainpid(pid_t pid)
{
	(void)pid;
}

#endif /* HAVE_SYSTEMD */

#endif /* OD_SYSTEMD_NOTIFY_H */

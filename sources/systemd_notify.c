/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <unistd.h>
#include <systemd_notify.h>

#ifdef HAVE_SYSTEMD

#include <systemd/sd-daemon.h>

void od_systemd_notify_ready(void)
{
	sd_notify(0, "READY=1\n"
		     "STATUS=Ready to accept connections");
}

void od_systemd_notify_reloading(const char *msg)
{
	char buff[256];
	snprintf(buff, sizeof(buff), "RELOADING=1\nSTATUS=%s", msg);

	sd_notify(0, buff);
}

void od_systemd_notify_stopping(void)
{
	sd_notify(0, "STOPPING=1\n"
		     "STATUS=Shutting down gracefully");
}

void od_systemd_notify_mainpid(pid_t pid)
{
	char msg[64];
	snprintf(msg, sizeof(msg), "MAINPID=%d", (int)pid);
	sd_notify(0, msg);
}

#endif /* HAVE_SYSTEMD */

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
#endif

void od_systemd_notify_ready(void)
{
#ifdef HAVE_SYSTEMD
	/* Check if we're running under systemd */
	if (getenv("NOTIFY_SOCKET") != NULL) {
		sd_notify(0, "READY=1\n"
			     "STATUS=Ready to accept connections");
	}
#endif
}

void od_systemd_notify_reloading(void)
{
#ifdef HAVE_SYSTEMD
	if (getenv("NOTIFY_SOCKET") != NULL) {
		sd_notify(0, "RELOADING=1\n"
			     "STATUS=Reloading configuration");
	}
#endif
}

void od_systemd_notify_stopping(void)
{
#ifdef HAVE_SYSTEMD
	if (getenv("NOTIFY_SOCKET") != NULL) {
		sd_notify(0, "STOPPING=1\n"
			     "STATUS=Shutting down gracefully");
	}
#endif
}

void od_systemd_notify_mainpid(pid_t pid)
{
#ifdef HAVE_SYSTEMD
	if (getenv("NOTIFY_SOCKET") != NULL) {
		char msg[64];
		snprintf(msg, sizeof(msg), "MAINPID=%d", (int)pid);
		sd_notify(0, msg);
	}
#endif
}

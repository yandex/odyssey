/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <sys/inotify.h>
#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <tls_watcher.h>
#include <logger.h>
#include <system.h>

DEFINE_SIMPLE_MODULE_LOGGER(tw, "tls-watcher")

static inline void tw_watcher(void *arg)
{
	od_tls_watcher_t *watcher = arg;
	
	tw_log("TLS watcher started, monitoring %d file(s)", watcher->watch_count);
	
	char event_buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	
	while (1) {
		/* Check stop flag with 1 second timeout */
		int rc = machine_wait_flag_wait(watcher->stop_flag, 1000);
		if (rc != -1 && machine_errno() != ETIMEDOUT) {
			tw_log("stop flag is set, exiting TLS watcher");
			break;
		}
		
		/* Poll inotify fd for events (non-blocking) */
		struct pollfd pfd = {
			.fd = watcher->inotify_fd,
			.events = POLLIN
		};
		
		rc = poll(&pfd, 1, 0);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			}
			tw_error("poll failed: %s", strerror(errno));
			break;
		}
		
		if (rc == 0) {
			/* No events */
			continue;
		}
		
		/* Read inotify events */
		ssize_t len = read(watcher->inotify_fd, event_buf, sizeof(event_buf));
		if (len == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			}
			tw_error("read inotify failed: %s", strerror(errno));
			break;
		}
		
		/* Process events */
		const struct inotify_event *event;
		for (char *ptr = event_buf; ptr < event_buf + len;
		     ptr += sizeof(struct inotify_event) + event->len) {
			event = (const struct inotify_event *)ptr;
			
			/* Find which file was modified */
			const char *modified_file = NULL;
			for (int i = 0; i < watcher->watch_count; i++) {
				if (watcher->watches[i].wd == event->wd) {
					modified_file = watcher->watches[i].path;
					break;
				}
			}
			
			/* Handle certificate file modification */
			if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO)) {
				if (modified_file) {
					tw_log("TLS certificate file changed: %s", modified_file);
					
					/* Debounce: only reload if at least 1 second has passed */
					uint64_t now = machine_time_us();
					if (now - watcher->last_reload_time_us < 1000000) {
						/* Less than 1 second since last reload, skip */
						continue;
					}
					
					tw_log("triggering TLS reload");
					watcher->last_reload_time_us = now;
					
					/* Trigger TLS reload */
					od_system_t *system = watcher->system;
					od_system_tls_reload(system);
				}
			}
		}
	}
	
	tw_log("TLS watcher finished");
}

int od_tls_watcher_init(od_tls_watcher_t *watcher, void *system)
{
	memset(watcher, 0, sizeof(od_tls_watcher_t));
	
	watcher->system = system;
	watcher->watch_count = 0;
	watcher->last_reload_time_us = 0;
	
	/* Initialize inotify */
	watcher->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (watcher->inotify_fd == -1) {
		return -1;
	}
	
	/* Create stop flag */
	watcher->stop_flag = machine_wait_flag_create();
	if (watcher->stop_flag == NULL) {
		close(watcher->inotify_fd);
		return -1;
	}
	
	return 0;
}

int od_tls_watcher_add(od_tls_watcher_t *watcher, const char *path)
{
	if (watcher->watch_count >= OD_TLS_WATCHER_MAX_WATCHES) {
		tw_error("maximum watch count reached");
		return -1;
	}
	
	if (!path) {
		return 0; /* Nothing to watch */
	}
	
	/* Check if already watching this path */
	for (int i = 0; i < watcher->watch_count; i++) {
		if (strcmp(watcher->watches[i].path, path) == 0) {
			return 0; /* Already watching */
		}
	}
	
	/* Add inotify watch */
	int wd = inotify_add_watch(watcher->inotify_fd, path,
				    IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO);
	if (wd == -1) {
		return -1;
	}
	
	/* Store watch info */
	watcher->watches[watcher->watch_count].path = strdup(path);
	if (!watcher->watches[watcher->watch_count].path) {
		inotify_rm_watch(watcher->inotify_fd, wd);
		return -1;
	}
	watcher->watches[watcher->watch_count].wd = wd;
	watcher->watch_count++;
	
	return 0;
}

int od_tls_watcher_start(od_tls_watcher_t *watcher)
{
	if (watcher->watch_count == 0) {
		tw_log("no files to watch, not starting TLS watcher");
		return 0;
	}
	
	/* Start watcher thread */
	char name[32];
	snprintf(name, sizeof(name), "tls-watcher");
	watcher->worker_id = machine_create(name, tw_watcher, watcher);
	if (watcher->worker_id == -1) {
		tw_error("failed to create TLS watcher thread");
		return -1;
	}
	
	return 0;
}

void od_tls_watcher_destroy(od_tls_watcher_t *watcher)
{
	if (!watcher) {
		return;
	}
	
	/* Signal stop */
	if (watcher->stop_flag) {
		machine_wait_flag_set(watcher->stop_flag);
		
		/* Wait for thread to finish */
		if (watcher->worker_id != 0) {
			machine_wait(watcher->worker_id);
		}
	}
	
	/* Remove all watches */
	for (int i = 0; i < watcher->watch_count; i++) {
		if (watcher->watches[i].wd != -1) {
			inotify_rm_watch(watcher->inotify_fd, watcher->watches[i].wd);
		}
		if (watcher->watches[i].path) {
			free(watcher->watches[i].path);
		}
	}
	
	/* Close inotify */
	if (watcher->inotify_fd != -1) {
		close(watcher->inotify_fd);
	}
	
	tw_log("TLS watcher destroyed");
}

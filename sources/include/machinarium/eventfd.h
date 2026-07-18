#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/fd.h>
#include <machinarium/cond.h>
#include <machinarium/io.h>

typedef struct {
	mm_fd_t handle;
#if !defined(__linux__)
	int write_fd;
#endif
	mm_cond_t cond;
	int attached;
} mm_eventfd_t;

int mm_eventfd_init(mm_eventfd_t *efd);
void mm_eventfd_destroy(mm_eventfd_t *efd);

int mm_eventfd_attach(mm_eventfd_t *efd);
int mm_eventfd_detach(mm_eventfd_t *efd);

int mm_eventfd_read(mm_eventfd_t *efd, uint32_t timeout_ms);
int mm_eventfd_write(mm_eventfd_t *efd, uint32_t timeout_ms);

void mm_eventfd_peer_to(mm_eventfd_t *efd, mm_io_t *io);
void mm_eventfd_remove_peer_to(mm_eventfd_t *efd, mm_io_t *io);

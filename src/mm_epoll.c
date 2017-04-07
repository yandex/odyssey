
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

typedef struct mm_epoll_t mm_epoll_t;

struct mm_epoll_t {
	mm_poll_t poll;
	mm_list_t list;
};

static mm_poll_t*
mm_epoll_create(void)
{
	mm_epoll_t *epoll;
	epoll = malloc(sizeof(mm_epoll_t));
	if (epoll == NULL)
		return NULL;
	epoll->poll.iface = &mm_epoll_if;
	mm_list_init(&epoll->list);
	return &epoll->poll;
}

static void
mm_epoll_free(mm_poll_t *poll)
{
	free(poll);
}

static int
mm_epoll_shutdown(mm_poll_t *poll)
{
	(void)poll;
	return 0;
}

static int
mm_epoll_step(mm_poll_t *poll)
{
	(void)poll;
	return 0;
}

static int
mm_epoll_add(mm_poll_t *poll, mm_fd_t *fd, int mask)
{
	(void)poll;
	(void)fd;
	(void)mask;
	return 0;
}

static int
mm_epoll_modify(mm_poll_t *poll, mm_fd_t *fd, int mask)
{
	(void)poll;
	(void)fd;
	(void)mask;
	return 0;
}

static int
mm_epoll_del(mm_poll_t *poll, mm_fd_t *fd)
{
	(void)poll;
	(void)fd;
	return 0;
}

mm_pollif_t mm_epoll_if =
{
	.name     = "epoll",
	.create   = mm_epoll_create,
	.free     = mm_epoll_free,
	.shutdown = mm_epoll_shutdown,
	.step     = mm_epoll_step,
	.add      = mm_epoll_add,
	.modify   = mm_epoll_modify,
	.del      = mm_epoll_del
};

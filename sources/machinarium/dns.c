/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/mm.h>
#include <machinarium/socket.h>
#include <machinarium/dns.h>

static mm_addrinfo_t *addrinfo_clone(struct addrinfo *ai)
{
	mm_addrinfo_t *head = NULL;
	mm_addrinfo_t *tail = NULL;

	while (ai) {
		mm_addrinfo_t *node = mm_malloc(sizeof(mm_addrinfo_t));
		if (node == NULL) {
			goto error;
		}
		memset(node, 0, sizeof(mm_addrinfo_t));

		atomic_init(&node->refs, 1);

		node->ai_socktype = ai->ai_socktype;
		node->ai_protocol = ai->ai_protocol;
		node->ai_flags = ai->ai_flags;
		node->ai_family = ai->ai_family;
		node->ai_addrlen = ai->ai_addrlen;

		if (ai->ai_addr != NULL && ai->ai_addrlen > 0) {
			node->ai_addr = mm_malloc(ai->ai_addrlen);
			if (node->ai_addr == NULL) {
				mm_free(node);
				goto error;
			}
			memcpy(node->ai_addr, ai->ai_addr, ai->ai_addrlen);
		}

		if (ai->ai_canonname != NULL) {
			node->ai_canonname = mm_strdup(ai->ai_canonname);
			if (node->ai_canonname == NULL) {
				mm_free(node->ai_addr);
				mm_free(node);
				goto error;
			}
		}

		node->ai_next = NULL;

		if (tail != NULL) {
			tail->ai_next = node;
		} else {
			head = node;
		}
		tail = node;

		ai = ai->ai_next;
	}

	return head;

error:
	mm_freeaddrinfo(head);
	return NULL;
}

void mm_freeaddrinfo(mm_addrinfo_t *ai)
{
	if (ai == NULL) {
		return;
	}

	uint64_t r = atomic_fetch_sub(&ai->refs, 1);
	if (r > 1) {
		return;
	}

	while (ai) {
		mm_addrinfo_t *next = ai->ai_next;
		mm_free(ai->ai_addr);
		mm_free(ai->ai_canonname);
		mm_free(ai);
		ai = next;
	}
}

void mm_free_dns_cache_entry(mm_dns_cache_entry_t *e)
{
	mm_list_unlink(&e->link);
	if (e->ai) {
		mm_freeaddrinfo(e->ai);
		e->ai = NULL;
	}
	mm_free(e);
}

typedef struct {
	char *addr;
	char *service;
	struct addrinfo *hints;
	mm_addrinfo_t **res;
	int rc;
} mm_getaddrinfo_t;

static int entry_eq(const mm_getaddrinfo_t *arg, const mm_dns_cache_entry_t *e)
{
	if (arg->hints != NULL) {
		if (arg->hints->ai_socktype != e->ai_socktype) {
			return 0;
		}

		if (arg->hints->ai_protocol != e->ai_protocol) {
			return 0;
		}

		if (arg->hints->ai_family != e->ai_family) {
			return 0;
		}
	} else {
		/*
		 * from man getaddrinfo:
		 *
		 * Specifying hints as NULL is equivalent to setting
		 * ai_socktype and ai_protocol to 0; ai_family to AF_UNSPEC; and ai_flags to
       	 * (AI_V4MAPPED | AI_ADDRCONFIG).
		 */
		if (e->ai_socktype != 0 || e->ai_protocol != 0 ||
		    e->ai_family != AF_UNSPEC) {
			return 0;
		}
	}

	if (arg->addr == NULL) {
		if (strlen(e->addr) > 0) {
			return 0;
		}
	} else {
		if (strcmp(arg->addr, e->addr) != 0) {
			return 0;
		}
	}

	if (arg->service == NULL) {
		if (strlen(e->service) > 0) {
			return 0;
		}
	} else {
		if (strcmp(arg->service, e->service) != 0) {
			return 0;
		}
	}

	return 1;
}

static void mm_getaddrinfo_cb(mm_taskmgr_worker_t *worker, void *arg)
{
	mm_getaddrinfo_t *gai = arg;
	mm_dns_cache_entry_t *e = NULL;
	uint64_t now_ms = machine_time_ms();

	mm_list_t *i, *s;
	mm_list_foreach_safe (&worker->dns_cache, i, s) {
		mm_dns_cache_entry_t *c;
		c = mm_container_of(i, mm_dns_cache_entry_t, link);

		if (entry_eq(gai, c)) {
			e = c;
			break;
		}

		if (c->valid_until_ms <= now_ms) {
			mm_free_dns_cache_entry(c);
		}
	}

	if (e != NULL && e->valid_until_ms > now_ms) {
		/* valid value, reuse it */
		gai->rc = e->gai_rc;
		if (e->gai_rc == 0) {
			atomic_fetch_add(&e->ai->refs, 1);
			*gai->res = e->ai;
		}
		return;
	}

	if (e != NULL) {
		/* expired, free only gai result and reuse entry object */
		mm_freeaddrinfo(e->ai);
		e->ai = NULL;
	} else {
		/* unknown key */
		e = mm_malloc(sizeof(mm_dns_cache_entry_t));
		if (e == NULL) {
			gai->rc = -1;
			return;
		}
		memset(e, 0, sizeof(mm_dns_cache_entry_t));

		mm_list_init(&e->link);
		mm_list_append(&worker->dns_cache, &e->link);

		if (gai->addr != NULL) {
			strncpy(e->addr, gai->addr, sizeof(e->addr) - 1);
		}

		if (gai->service != NULL) {
			strncpy(e->service, gai->service,
				sizeof(e->service) - 1);
		}

		if (gai->hints != NULL) {
			e->ai_family = gai->hints->ai_family;
			e->ai_protocol = gai->hints->ai_protocol;
			e->ai_socktype = gai->hints->ai_socktype;
		}
	}

	struct addrinfo *ai = NULL;
	e->gai_rc =
		mm_socket_getaddrinfo(gai->addr, gai->service, gai->hints, &ai);
	gai->rc = e->gai_rc;
	e->valid_until_ms = machine_time_ms() + worker->dns_ttl_ms;
	if (gai->rc == 0) {
		mm_addrinfo_t *copy = addrinfo_clone(ai);
		freeaddrinfo(ai);
		if (copy == NULL) {
			e->valid_until_ms = 0;
			gai->rc = -1;
			return;
		}
		atomic_fetch_add(&copy->refs, 1);
		*gai->res = copy;
		e->ai = copy;
	}
}

int mm_getaddrinfo(char *addr, char *service, struct addrinfo *hints,
		   mm_addrinfo_t **res, uint32_t time_ms)
{
	mm_getaddrinfo_t gai = { .addr = addr,
				 .service = service,
				 .hints = hints,
				 .res = res,
				 .rc = 0 };
	int rc;
	rc = mm_taskmgr_new(&machinarium.task_mgr, mm_getaddrinfo_cb, &gai,
			    time_ms);
	if (rc == -1) {
		return -1;
	}
	return gai.rc;
}

MACHINE_API int machine_getsockname(mm_io_t *obj, struct sockaddr *sa,
				    int *salen)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);
	socklen_t slen = *salen;
	int rc = mm_socket_getsockname(io->fd, sa, &slen);
	if (rc < 0) {
		mm_errno_set(errno);
		return -1;
	}
	*salen = slen;
	return 0;
}

MACHINE_API int machine_getpeername(mm_io_t *obj, struct sockaddr *sa,
				    int *salen)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	mm_errno_set(0);
	socklen_t slen = *salen;
	int rc = mm_socket_getpeername(io->fd, sa, &slen);
	if (rc < 0) {
		mm_errno_set(errno);
		return -1;
	}
	*salen = slen;
	return 0;
}

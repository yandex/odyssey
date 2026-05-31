#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <stdatomic.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <machinarium/list.h>

typedef struct mm_addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	socklen_t ai_addrlen;
	struct sockaddr *ai_addr;
	char *ai_canonname;
	struct mm_addrinfo *ai_next;

	atomic_uint_fast64_t refs;
} mm_addrinfo_t;

typedef struct {
	char addr[NI_MAXHOST];
	char service[NI_MAXSERV];
	int ai_family;
	int ai_socktype;
	int ai_protocol;

	int gai_rc;
	mm_addrinfo_t *ai;

	uint64_t valid_until_ms;

	mm_list_t link;
} mm_dns_cache_entry_t;

void mm_free_dns_cache_entry(mm_dns_cache_entry_t *e);

int mm_getaddrinfo(char *addr, char *service, struct addrinfo *hints,
		   mm_addrinfo_t **res, uint32_t time_ms);

void mm_freeaddrinfo(mm_addrinfo_t *ai);

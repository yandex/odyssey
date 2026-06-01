#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>
#include <machinarium/dns.h>

int od_getsockaddrname(struct sockaddr *, char *, int, int, int);
int od_getaddrname(mm_addrinfo_t *, char *, int, int, int);
int od_getpeername(mm_io_t *, char *, int, int, int);
int od_getsockname(mm_io_t *, char *, int, int, int);

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_getsockaddrname(struct sockaddr *, char *, int, int, int);
int od_getaddrname(struct addrinfo *, char *, int, int, int);
int od_getpeername(machine_io_t *, char *, int, int, int);
int od_getsockname(machine_io_t *, char *, int, int, int);

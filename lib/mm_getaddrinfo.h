#ifndef MM_GETADDRINFO_H_
#define MM_GETADDRINFO_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mmaddrinfo mmaddrinfo;

struct mmaddrinfo {
	struct addrinfo *res;
};

#endif

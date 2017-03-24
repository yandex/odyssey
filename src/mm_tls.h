#ifndef MM_TLS_H_
#define MM_TLS_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_tls_t mm_tls_t;

struct mm_tls_t {
	struct tls_config *config;
};

#endif

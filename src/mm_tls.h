#ifndef MM_TLS_H_
#define MM_TLS_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_tls_t mm_tls_t;

typedef enum {
	MM_TLS_NONE,
	MM_TLS_PEER,
	MM_TLS_PEER_STRICT
} mm_tlsverify_t;

struct mm_tls_t {
	mm_tlsverify_t verify;
	char          *server;
	char          *protocols;
	char          *ca_path;
	char          *ca_file;
	char          *cert_file;
	char          *key_file;
};

#endif

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
	MM_TLS_ALLOW,
	MM_TLS_REQUIRE,
	MM_TLS_FULL
} mm_tlsmode_t;

struct mm_tls_t {
	mm_tlsmode_t mode;
	char        *protocols;
	char        *ca_path;
	char        *ca_file;
	char        *cert_file;
	char        *key_file;
};

#endif

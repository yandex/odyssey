#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * some helpers for working with prepared statements
 *
 * for pstmt support implementation there must be next hashmaps created
 *
 * for client:
 * 	client pstmt name -> *pstmt description
 *
 * for server:
 * 	server pstmt name -> *pstmt description
 *
 * global:
 * 	pstmt description -> ref counter
 *
 * ex.:
 * client sends parse "P0" with query "Q"
 * the description will be "Q" (+ param bytes)
 * client name will be "P0"
 * hash will be murmur("Q" + param bytes)
 * server name will be "pstmt_#" where # is global counter
 */

#include <machinarium/ds/hm.h>

#include <types.h>

#define OD_PSTMT_NAME_PREFIX "odyssey_pstmt_"
#define OD_PSTMT_NAME_MAX \
	(sizeof(OD_PSTMT_NAME_PREFIX) + sizeof("18446744073709551615") + 1)

typedef char od_pstmt_name_t[OD_PSTMT_NAME_MAX];

/* query and param bytes from Parse message */
typedef struct {
	void *data;
	size_t len;
} od_pstmt_desc_t;

struct od_pstmt {
	od_pstmt_desc_t desc;
	od_pstmt_name_t name;
};

/* "P_0" -> *od_pstmt_t */
mm_hashmap_t *od_client_pstmt_hashmap_create(void);
void od_client_pstmt_hashmap_free(mm_hashmap_t *hm);

int od_client_add_pstmt(od_client_t *client, const char *name,
			const od_pstmt_t *pstmt);
int od_client_has_pstmt(od_client_t *client, const char *name);
int od_client_remove_pstmt(od_client_t *client, const char *name);
od_pstmt_t *od_client_get_pstmt(od_client_t *client, const char *name);
void od_client_pstmts_clear(od_client_t *client);

/* "odyssey_pstmt_0" -> *od_pstmt_t */
mm_hashmap_t *od_server_pstmt_hashmap_create(void);
void od_server_pstmt_hashmap_free(mm_hashmap_t *hm);

int od_server_has_pstmt(od_server_t *server, const od_pstmt_t *pstmt);
int od_server_add_pstmt(od_server_t *server, const od_pstmt_t *pstmt);
int od_server_remove_pstmt(od_server_t *server, const od_pstmt_t *pstmt);
void od_server_pstmts_clear(od_server_t *server);

/* od_pstmt_desc_t -> od_pstmt_t */
mm_hashmap_t *od_global_pstmts_map_create(void);
void od_global_pstmts_map_free(mm_hashmap_t *hm);

void od_pstmt_next_name(od_pstmt_t *out);

od_pstmt_t *od_pstmt_create_or_get(mm_hashmap_t *gm, od_pstmt_desc_t desc);

/* helpers */
char *od_pstmt_name_from_parse(machine_msg_t *msg);
od_pstmt_desc_t od_pstmt_desc_from_parse(machine_msg_t *msg);
od_pstmt_desc_t od_pstmt_desc_copy(const od_pstmt_desc_t desc);

machine_msg_t *od_pstmt_parse_of(const od_pstmt_t *pstmt);
machine_msg_t *od_pstmt_describe_of(const od_pstmt_t *pstmt);

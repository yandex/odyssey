/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>
#include <machinarium/machinarium.h>

#include <od_memory.h>
#include <global.h>
#include <client.h>
#include <instance.h>
#include <murmurhash.h>
#include <util.h>
#include <client.h>
#include <server.h>
#include <pstmt.h>

/*
 * client hash map
 * "P_0" -> *od_prepared_stmt_t
 */

static mm_hash_t murmur_str_ptr(const void *data)
{
	const char *ptr = *(const char **)data;

	return (mm_hash_t)od_murmur_hash(ptr, strlen(ptr));
}

static int str_ptr_cmp(const void *k1, const void *k2)
{
	const char *s1 = *(const char **)k1;
	const char *s2 = *(const char **)k2;

	return strcmp(s1, s2);
}

static void str_ptr_dtor(void *k)
{
	od_free(*(char **)k);
}

static int str_ptr_copy(void *dst, const void *src)
{
	const char *key = *(const char **)src;

	char *copy = od_strdup(key);
	if (copy == NULL) {
		return -1;
	}

	memcpy(dst, &copy, sizeof(char *));

	return 0;
}

mm_hashmap_t *od_client_pstmt_hashmap_create(void)
{
	return mm_hashmap_create(
		50 /* XXX: big enough? */,
		1 /* nlocks = 1, no fully-concurrent access to server hashmap */,
		sizeof(char *), sizeof(od_pstmt_t *), str_ptr_cmp,
		murmur_str_ptr, str_ptr_dtor,
		NULL /* no need to free the pointer from global table */,
		str_ptr_copy);
}

void od_client_pstmt_hashmap_free(mm_hashmap_t *hm)
{
	mm_hashmap_free(hm);
}

od_pstmt_t *od_client_get_pstmt(od_client_t *client, const char *name)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(client->prep_stmt_ids, &klock, &name,
				     0 /* do not create */);
	(void)rc;

	if (klock.found) {
		od_pstmt_t *ps = *(od_pstmt_t **)mm_hashmap_kvp_val(
			client->prep_stmt_ids, klock.kvp);
		/* no real concurrent access - can unlock now and return */
		mm_hashmap_unlock_key(client->prep_stmt_ids, &klock);
		return ps;
	}

	/* lock is not held if key not found */
	return NULL;
}

int od_client_add_pstmt(od_client_t *client, const char *name,
			const od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(client->prep_stmt_ids, &klock, &name,
				     1 /* do create */);
	if (rc == -1) {
		/* cant create and lock is not held */
		return rc;
	}

	int ret = 0;

	if (!klock.found || name[0] == '\0') {
		/*
		 * new element - set the value
		 *
		 * if already exists - rewrite value only if name is ""
		 * (its PG specific for "" statemnt - unnamed statemnt is
		 * rewritten every Parse, not generate ErrorResponse about
		 * statement already exists)
		 */
		void *val =
			mm_hashmap_kvp_val(client->prep_stmt_ids, klock.kvp);
		memcpy(val, &pstmt, sizeof(const od_pstmt_t *));

		ret = 0;
	} else {
		/* value already exists and name != "" */
		ret = 1;
	}

	/* no real concurrent access - can unlock now and return */
	mm_hashmap_unlock_key(client->prep_stmt_ids, &klock);

	return ret;
}

int od_client_has_pstmt(od_client_t *client, const char *name)
{
	return od_client_get_pstmt(client, name) != NULL;
}

int od_client_remove_pstmt(od_client_t *client, const char *name)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(client->prep_stmt_ids, &klock, &name,
				     0 /* do not create */);
	(void)rc;

	if (klock.kvp != NULL) {
		mm_hashmap_remove(client->prep_stmt_ids, &klock);
	}

	/* if not found - lock is not held */

	return 0;
}

void od_client_pstmts_clear(od_client_t *client)
{
	mm_hashmap_clear(client->prep_stmt_ids);
}

/*
 * server hashmap
 * "odyssey_pstmt_0" -> *od_prepared_stmt_t
 */

static mm_hash_t murmur_str_inplace(const void *data)
{
	const char *key = data;

	return (mm_hash_t)od_murmur_hash(data, strlen(key));
}

static int str_inplace_cmp(const void *k1, const void *k2)
{
	const char *s1 = k1;
	const char *s2 = k2;

	return strcmp(s1, s2);
}

mm_hashmap_t *od_server_pstmt_hashmap_create(void)
{
	return mm_hashmap_create(
		100 /* XXX: big enough? */,
		1 /* nlocks = 1, no fully-concurrent access to server hashmap */,
		sizeof(od_pstmt_name_t), sizeof(od_pstmt_t *), str_inplace_cmp,
		murmur_str_inplace, NULL /* no need to free on inplace str */,
		NULL /* no need to free the pointer from global table */,
		NULL /* no key copy function */
	);
}

void od_server_pstmt_hashmap_free(mm_hashmap_t *hm)
{
	mm_hashmap_free(hm);
}

int od_server_has_pstmt(od_server_t *server, const od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(server->prep_stmts, &klock, pstmt->name,
				     0 /* do not create */);
	(void)rc;

	if (klock.found) {
		/* no real concurrent access - can unlock now and return */
		mm_hashmap_unlock_key(server->prep_stmts, &klock);
		return 1;
	}

	/* not exists - no lock held */
	return 0;
}

int od_server_add_pstmt(od_server_t *server, const od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(server->prep_stmts, &klock, pstmt->name,
				     1 /* do create */);
	if (rc == -1) {
		/* cant create and lock is not held */
		return rc;
	}

	int ret = 0;

	if (!klock.found) {
		/* new element - set the value */
		void *val = mm_hashmap_kvp_val(server->prep_stmts, klock.kvp);
		memcpy(val, &pstmt, sizeof(const od_pstmt_t *));

		ret = 0;
	} else {
		ret = 1;
	}

	/* no real concurrent access - can unlock now and return */
	mm_hashmap_unlock_key(server->prep_stmts, &klock);

	return ret;
}

int od_server_remove_pstmt(od_server_t *server, const od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(server->prep_stmts, &klock, pstmt->name,
				     0 /* do not create */);
	(void)rc;

	if (klock.found) {
		mm_hashmap_remove(server->prep_stmts, &klock);
	}

	/* if not found - lock is not held */

	return 0;
}

void od_server_pstmts_clear(od_server_t *server)
{
	mm_hashmap_clear(server->prep_stmts);
}

/*
 * global map
 * od_pstmt_desc_t -> od_pstmt_t
 */

static mm_hash_t murmur_pstmt_desc(const void *data)
{
	const od_pstmt_desc_t *desc = data;

	return (mm_hash_t)od_murmur_hash(desc->data, desc->len);
}

static int pstmt_desc_cmp(const void *k1, const void *k2)
{
	const od_pstmt_desc_t *d1 = k1;
	const od_pstmt_desc_t *d2 = k2;

	if (d1->len != d2->len) {
		return 1;
	}

	return memcmp(d1->data, d2->data, d1->len);
}

static void pstmt_desc_free(void *k)
{
	od_pstmt_desc_t *desc = k;
	od_free(desc->data);
}

static int pstmt_desc_copy_cb(void *dest, const void *src)
{
	const od_pstmt_desc_t *orig = src;
	od_pstmt_desc_t copy = od_pstmt_desc_copy(*orig);
	if (copy.data == NULL) {
		return 1;
	}

	memcpy(dest, &copy, sizeof(od_pstmt_desc_t));

	return 0;
}

mm_hashmap_t *od_global_pstmts_map_create(void)
{
	return mm_hashmap_create(1000 /* XXX: big enough? */,
				 1000 /* XXX: big enough? */,
				 sizeof(od_pstmt_desc_t), sizeof(od_pstmt_t),
				 pstmt_desc_cmp, murmur_pstmt_desc,
				 pstmt_desc_free, NULL, pstmt_desc_copy_cb);
}

void od_global_pstmts_map_free(mm_hashmap_t *hm)
{
	mm_hashmap_free(hm);
}

od_pstmt_t *od_pstmt_create_or_get(mm_hashmap_t *pstmts,
				   const od_pstmt_desc_t desc)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(pstmts, &klock, &desc,
				     1 /* create if not exists */);
	if (rc == -1) {
		return NULL;
	}

	void *value = mm_hashmap_kvp_val(pstmts, klock.kvp);

	if (!klock.found) {
		/* init new prep stmt */
		od_pstmt_t pstmt;
		memset(&pstmt, 0, sizeof(od_pstmt_t));
		od_pstmt_next_name(&pstmt);

		/* let the value points to key */
		pstmt.desc = *((od_pstmt_desc_t *)mm_hashmap_kvp_key(
			pstmts, klock.kvp));

		memcpy(value, &pstmt, sizeof(od_pstmt_t));
	}

	mm_hashmap_unlock_key(pstmts, &klock);

	return value;
}

/* helpers */

char *od_pstmt_name_from_parse(machine_msg_t *msg)
{
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);
	kiwi_prepared_statement_t desc;
	if (kiwi_be_read_parse_dest(data, size, &desc)) {
		return NULL;
	}

	return desc.operator_name;
}

od_pstmt_desc_t od_pstmt_desc_from_parse(machine_msg_t *msg)
{
	od_pstmt_desc_t ret;
	memset(&ret, 0, sizeof(od_pstmt_desc_t));

	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);
	kiwi_prepared_statement_t desc;
	if (kiwi_be_read_parse_dest(data, size, &desc) == 0) {
		ret.data = desc.description;
		ret.len = desc.description_len;
	}

	return ret;
}

machine_msg_t *od_pstmt_parse_of(const od_pstmt_t *pstmt)
{
	const char *srv_name = pstmt->name;
	const od_pstmt_desc_t *desc = &pstmt->desc;

	machine_msg_t *pmsg = kiwi_fe_write_parse_description(
		NULL, srv_name, strlen(srv_name) + 1 /* include zero byte */,
		desc->data, desc->len);

	return pmsg;
}

machine_msg_t *od_pstmt_describe_of(const od_pstmt_t *pstmt)
{
	const char *srv_name = pstmt->name;

	machine_msg_t *dmsg = kiwi_fe_write_describe(NULL, 'S', srv_name,
						     strlen(srv_name) + 1);

	return dmsg;
}

od_pstmt_desc_t od_pstmt_desc_copy(const od_pstmt_desc_t desc)
{
	if (desc.data == NULL) {
		return desc;
	}

	od_pstmt_desc_t copy;
	memset(&copy, 0, sizeof(od_pstmt_desc_t));

	copy.data = od_malloc(desc.len);
	if (copy.data != NULL) {
		copy.len = desc.len;
		memcpy(copy.data, desc.data, desc.len);
		return copy;
	}

	return copy;
}

void od_pstmt_next_name(od_pstmt_t *out)
{
	static atomic_uint_fast64_t cnt = 0;

	uint64_t num = atomic_fetch_add(&cnt, 1);

	od_snprintf(out->name, sizeof(od_pstmt_name_t), "%s%" PRIu64,
		    OD_PSTMT_NAME_PREFIX, num);
}

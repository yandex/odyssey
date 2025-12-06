#pragma once
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_cancel(od_global_t *, od_rule_storage_t *, const od_address_t *,
	      kiwi_key_t *, od_id_t *);

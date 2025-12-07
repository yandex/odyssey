#pragma once
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi/kiwi.h>

#include <types.h>
#include <storage.h>
#include <id.h>

int od_cancel(od_global_t *, od_rule_storage_t *, const od_address_t *,
	      kiwi_key_t *, od_id_t *);

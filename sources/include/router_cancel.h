#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <id.h>
#include <storage.h>
#include <address.h>

#include <kiwi/kiwi.h>

typedef struct {
	od_id_t id;
	od_rule_storage_t *storage;
	const od_address_t *address;
	kiwi_key_t key;
} od_router_cancel_t;

static inline void od_router_cancel_init(od_router_cancel_t *cancel)
{
	cancel->storage = NULL;
	cancel->address = NULL;
	kiwi_key_init(&cancel->key);
}

static inline void od_router_cancel_free(od_router_cancel_t *cancel)
{
	if (cancel->storage) {
		od_rules_storage_free(cancel->storage);
	}
}

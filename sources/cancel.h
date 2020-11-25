#ifndef ODYSSEY_CANCEL_H
#define ODYSSEY_CANCEL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

int
od_cancel(od_global_t *, od_rule_storage_t *, kiwi_key_t *, od_id_t *);

#endif /* ODYSSEY_CANCEL_H */

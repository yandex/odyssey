#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/mutex.h>

#include <hba_rule.h>

typedef struct od_logger od_logger_t;

struct od_hba {
	mm_mutex_t lock;
	od_hba_rules_t rules;
};

void od_hba_init(od_hba_t *hba);
void od_hba_free(od_hba_t *hba);
void od_hba_reload(od_hba_t *hba, od_hba_rules_t *rules);
void od_hba_rules_print(od_hba_t *hba, od_logger_t *logger);
int od_hba_process(od_client_t *client);

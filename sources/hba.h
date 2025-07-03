#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

struct od_hba {
	pthread_mutex_t lock;
	od_hba_rules_t rules;
};

void od_hba_init(od_hba_t *hba);
void od_hba_free(od_hba_t *hba);
void od_hba_reload(od_hba_t *hba, od_hba_rules_t *rules);
int od_hba_process(od_client_t *client);

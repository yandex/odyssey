
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

void od_hba_init(od_hba_t *hba)
{
	pthread_mutex_init(&hba->lock, NULL);
	od_hba_rules_init(&hba->rules);
}

void od_hba_free(od_hba_t *hba)
{
	od_hba_rules_free(&hba->rules);
	pthread_mutex_destroy(&hba->lock);
}

void od_hba_lock(od_hba_t *hba)
{
	pthread_mutex_lock(&hba->lock);
}

void od_hba_unlock(od_hba_t *hba)
{
	pthread_mutex_unlock(&hba->lock);
}

void od_hba_reload(od_hba_t *hba, od_hba_rules_t *rules)
{
	od_hba_lock(hba);

	od_list_init(&hba->rules);
	memcpy(&hba->rules, &rules, sizeof(rules));

	od_hba_unlock(hba);
}

bool od_hba_validate_name(char *client_name, od_hba_rule_name_t *name,
			  char *client_other_name)
{
	if (name->flags & OD_HBA_NAME_ALL) {
		return true;
	}

	if ((name->flags & OD_HBA_NAME_SAMEUSER) &&
	    strcmp(client_name, client_other_name) == 0) {
		return true;
	}

	od_list_t *i;
	od_hba_rule_name_item_t *item;
	od_list_foreach(&name->values, i)
	{
		item = od_container_of(i, od_hba_rule_name_item_t, link);
		if (item->value != NULL &&
		    strcmp(client_name, item->value) == 0) {
			return true;
		}
	}

	return false;
}

int od_hba_process(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_hba_t *hba = client->global->hba;
	od_list_t *i;
	od_hba_rule_t *rule;
	od_hba_rules_t *rules;

	if (instance->config.hba_file == NULL) {
		return OK_RESPONSE;
	}

	struct sockaddr_storage sa;
	int salen = sizeof(sa);
	struct sockaddr *saddr = (struct sockaddr *)&sa;
	int rc = machine_getpeername(client->io.io, saddr, &salen);
	if (rc == -1)
		return -1;

	od_hba_lock(hba);
	rules = &hba->rules;
	od_hba_unlock(hba);

	od_list_foreach(rules, i)
	{
		rule = od_container_of(i, od_hba_rule_t, link);
		if (sa.ss_family == AF_UNIX) {
			if (rule->connection_type != OD_CONFIG_HBA_LOCAL)
				continue;
		} else if (rule->connection_type == OD_CONFIG_HBA_LOCAL) {
			continue;
		} else if (rule->connection_type == OD_CONFIG_HBA_HOSTSSL &&
			   !client->startup.is_ssl_request) {
			continue;
		} else if (rule->connection_type == OD_CONFIG_HBA_HOSTNOSSL &&
			   client->startup.is_ssl_request) {
			continue;
		} else if (sa.ss_family == AF_INET ||
			   sa.ss_family == AF_INET6) {
			if (!od_address_validate(&rule->address_range, &sa)) {
				continue;
			}
		}

		if (!od_hba_validate_name(client->rule->db_name,
					  &rule->database,
					  client->rule->user_name)) {
			continue;
		}
		if (!od_hba_validate_name(client->rule->user_name, &rule->user,
					  client->rule->db_name)) {
			continue;
		}

		rc = rule->auth_method == OD_CONFIG_HBA_ALLOW ? OK_RESPONSE :
								      NOT_OK_RESPONSE;

		return rc;
	}

	return NOT_OK_RESPONSE;
}

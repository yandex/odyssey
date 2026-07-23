
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <list.h>
#include <hba.h>
#include <hba_rule.h>
#include <client.h>
#include <global.h>
#include <instance.h>

void od_hba_init(od_hba_t *hba)
{
	mm_mutex_init(&hba->lock);
	od_hba_rules_init(&hba->rules);
}

void od_hba_free(od_hba_t *hba)
{
	od_hba_rules_free(&hba->rules);
	mm_mutex_destroy(&hba->lock);
}

void od_hba_lock(od_hba_t *hba)
{
	mm_mutex_lock2(&hba->lock);
}

void od_hba_unlock(od_hba_t *hba)
{
	mm_mutex_unlock(&hba->lock);
}

void od_hba_reload(od_hba_t *hba, od_hba_rules_t *rules)
{
	od_hba_lock(hba);

	od_hba_rules_free(&hba->rules);

	od_list_init(&hba->rules);

	od_list_t *i, *n;
	od_list_foreach_safe (rules, i, n) {
		od_hba_rule_t *rule;
		rule = od_container_of(i, od_hba_rule_t, link);

		od_hba_rules_add(&hba->rules, rule);
	}

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
	od_list_foreach (&name->values, i) {
		item = od_container_of(i, od_hba_rule_name_item_t, link);
		if (item->value != NULL &&
		    strcmp(client_name, item->value) == 0) {
			return true;
		}
	}

	return false;
}

void od_hba_rules_print(od_hba_t *hba, od_logger_t *logger)
{
	static const char *conn_type_str[] = {
		[OD_CONFIG_HBA_LOCAL] = "local",
		[OD_CONFIG_HBA_HOST] = "host",
		[OD_CONFIG_HBA_HOSTSSL] = "hostssl",
		[OD_CONFIG_HBA_HOSTNOSSL] = "hostnossl",
	};

	od_list_t *i;
	od_list_foreach (&hba->rules, i) {
		od_hba_rule_t *rule = od_container_of(i, od_hba_rule_t, link);

		/* database names */
		char db_buf[256];
		int db_pos = 0;
		if (rule->database.flags & OD_HBA_NAME_ALL) {
			db_pos += snprintf(db_buf + db_pos,
					   sizeof(db_buf) - db_pos, "all");
		} else if (rule->database.flags & OD_HBA_NAME_SAMEUSER) {
			db_pos += snprintf(db_buf + db_pos,
					   sizeof(db_buf) - db_pos, "sameuser");
		} else {
			od_list_t *j;
			od_list_foreach (&rule->database.values, j) {
				od_hba_rule_name_item_t *item = od_container_of(
					j, od_hba_rule_name_item_t, link);
				if (db_pos > 0 && db_buf[db_pos - 1] != '(') {
					db_pos += snprintf(
						db_buf + db_pos,
						sizeof(db_buf) - db_pos, ",");
				}
				db_pos += snprintf(db_buf + db_pos,
						   sizeof(db_buf) - db_pos,
						   "%s", item->value);
			}
		}
		if (db_pos == 0) {
			snprintf(db_buf, sizeof(db_buf), "(none)");
		}

		/* user names */
		char user_buf[256];
		int user_pos = 0;
		if (rule->user.flags & OD_HBA_NAME_ALL) {
			user_pos +=
				snprintf(user_buf + user_pos,
					 sizeof(user_buf) - user_pos, "all");
		} else if (rule->user.flags & OD_HBA_NAME_SAMEUSER) {
			user_pos += snprintf(user_buf + user_pos,
					     sizeof(user_buf) - user_pos,
					     "sameuser");
		} else {
			od_list_t *j;
			od_list_foreach (&rule->user.values, j) {
				od_hba_rule_name_item_t *item = od_container_of(
					j, od_hba_rule_name_item_t, link);
				if (user_pos > 0 &&
				    user_buf[user_pos - 1] != '(') {
					user_pos += snprintf(
						user_buf + user_pos,
						sizeof(user_buf) - user_pos,
						",");
				}
				user_pos +=
					snprintf(user_buf + user_pos,
						 sizeof(user_buf) - user_pos,
						 "%s", item->value);
			}
		}
		if (user_pos == 0) {
			snprintf(user_buf, sizeof(user_buf), "(none)");
		}

		const char *addr_str =
			(rule->connection_type == OD_CONFIG_HBA_LOCAL) ?
				"" :
			(rule->address_range.string_value != NULL) ?
				rule->address_range.string_value :
				"";

		od_log(logger, "config", NULL, NULL,
		       "hba rule  %-10s  db=%-20s  user=%-20s  addr=%-20s  %s",
		       conn_type_str[rule->connection_type], db_buf, user_buf,
		       addr_str,
		       rule->auth_method == OD_CONFIG_HBA_ALLOW ? "allow" :
								  "deny");
	}
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
	if (rc == -1) {
		return -1;
	}

	od_hba_lock(hba);
	rules = &hba->rules;
	od_hba_unlock(hba);

	od_list_foreach (rules, i) {
		rule = od_container_of(i, od_hba_rule_t, link);
		if (sa.ss_family == AF_UNIX) {
			if (rule->connection_type != OD_CONFIG_HBA_LOCAL) {
				continue;
			}
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

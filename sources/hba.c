
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

bool od_hba_validate_addr(od_config_hba_t *rule, struct sockaddr_storage *sa)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;
	struct sockaddr_in *rule_addr = (struct sockaddr_in *)&rule->addr;
	struct sockaddr_in *rule_mask = (struct sockaddr_in *)&rule->mask;
	in_addr_t client_addr = sin->sin_addr.s_addr;
	in_addr_t client_net = rule_mask->sin_addr.s_addr & client_addr;
	return (client_net ^ rule_addr->sin_addr.s_addr) == 0;
}

bool od_hba_validate_addr6(od_config_hba_t *rule, struct sockaddr_storage *sa)
{
	struct sockaddr_in6 *sin = (struct sockaddr_in6 *)sa;
	struct sockaddr_in6 *rule_addr = (struct sockaddr_in6 *)&rule->addr;
	struct sockaddr_in6 *rule_mask = (struct sockaddr_in6 *)&rule->mask;
	for (int i = 0; i < 16; ++i) {
		uint8_t client_net_byte = rule_mask->sin6_addr.s6_addr[i] & sin->sin6_addr.s6_addr[i];
		uint8_t res_byte = client_net_byte ^ rule_addr->sin6_addr.s6_addr[i];
		if (res_byte != 0)
			return false;
	}

	return true;
}

bool od_hba_validate_name(char *client_name, struct od_config_hba_name *name)
{
	if (name->flags & OD_HBA_NAME_ALL) {
		return true;
	}

	od_list_t *i;
	struct od_config_hba_name_item *item;
	od_list_foreach(&name->values, i)
	{
		item = od_container_of(i, struct od_config_hba_name_item, link);
		if (item->value != NULL && strcmp(client_name, item->value) == 0) {
			return true;
		}
	}

	return false;
}

bool od_hba_process_rule(od_client_t *client, od_config_hba_t *rule)
{
	struct sockaddr_storage sa;
	int salen = sizeof(sa);
	struct sockaddr *saddr = (struct sockaddr *)&sa;
	int rc = machine_getpeername(client->io.io, saddr, &salen);
	if (rc == -1)
		return false;
	if (sa.ss_family == AF_UNIX) {
		if (rule->connection_type != OD_CONFIG_HBA_LOCAL)
			return false;
	} else if (rule->connection_type == OD_CONFIG_HBA_LOCAL) {
		return false;
	} else if (rule->connection_type == OD_CONFIG_HBA_HOSTSSL && !client->startup.is_ssl_request) {
		return false;
	} else if (rule->connection_type == OD_CONFIG_HBA_HOSTNOSSL && client->startup.is_ssl_request) {
		return false;
	} else if (sa.ss_family == AF_INET) {
		if (rule->addr.ss_family != AF_INET || !od_hba_validate_addr(rule, &sa)) {
			return false;
		}
//		if (rule->addr.ss_family != AF_INET)
//			return false;
//		rc = od_hba_validate_addr(rule, &sa);
//		if (rc != 0)
//			return false;
	} else if (sa.ss_family == AF_INET6) {
		if (rule->addr.ss_family != AF_INET6 || !od_hba_validate_addr6(rule, &sa)) {
			return false;
		}
//		if (rule->addr.ss_family != AF_INET6)
//			return false;
//		struct sockaddr_in6 sin = *(struct sockaddr_in6 *)&sa;
//		rc = od_hba_validate_addr6(rule, &sa);
//		if (rc != 0)
//			return false;
	}

	if (!od_hba_validate_name(client->rule->db_name, &rule->database)) {
		return false;
	}
	if (!od_hba_validate_name(client->rule->user_name, &rule->user)) {
		return false;
	}

	return true;
}

int od_hba_process(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_list_t *i;
	od_config_hba_t *hba_rule;

	od_list_foreach(&instance->config.hba, i)
	{
		hba_rule = od_container_of(i, od_config_hba_t, link);
		if (od_hba_process_rule(client, hba_rule))
			return 0;
	}

	return -1;
}

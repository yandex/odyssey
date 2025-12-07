#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * Types definitions to use it in cycle references (like client <-> server or pool <-> server)
*/

typedef struct od_global od_global_t;
typedef struct od_instance od_instance_t;
typedef struct od_system_server od_system_server_t;
typedef struct od_router od_router_t;
typedef struct od_cron od_cron_t;
typedef struct od_worker_pool od_worker_pool_t;
typedef struct od_extension od_extension_t;
typedef struct od_hba od_hba_t;
typedef struct od_system od_system_t;
typedef struct od_address od_address_t;
typedef struct od_client od_client_t;
typedef struct od_client_pool od_client_pool_t;
typedef struct od_error_logger od_error_logger_t;
typedef struct od_server od_server_t;
typedef struct od_route od_route_t;
typedef struct od_server_pool od_server_pool_t;
typedef struct od_multi_pool_element od_multi_pool_element_t;
typedef struct od_multi_pool od_multi_pool_t;
typedef struct od_soft_oom_checker od_soft_oom_checker_t;
typedef struct od_config_listen od_config_listen_t;
typedef struct od_config_reader od_config_reader_t;
typedef struct od_config_online_restart_drop_options
	od_config_online_restart_drop_options_t;
typedef struct od_config od_config_t;
typedef struct od_config_soft_oom od_config_soft_oom_t;
typedef struct od_config_soft_oom_drop od_config_soft_oom_drop_t;
typedef struct od_relay od_relay_t;
typedef struct od_rule_auth od_rule_auth_t;
typedef struct od_rule od_rule_t;
typedef struct od_rules od_rules_t;

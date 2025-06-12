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

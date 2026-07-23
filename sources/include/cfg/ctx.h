#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>

#include <cfg/diag.h>
#include <cfg/model.h>

typedef struct od_cfg_parse_ctx {
	const char *filename;

	char *input;
	size_t input_size;

	int lexer_line;
	int lexer_column;
	size_t lexer_offset;

	od_cfg_model_t *model;
	od_cfg_diag_list_t *diags;
	int include_depth;

	/*
	 * bison scopes
	 */
	od_cfg_listen_t *current_listen;
	od_cfg_shared_pool_t *current_shared_pool;
	od_cfg_conn_drop_options_t *current_conn_drop_options;
	od_cfg_storage_t *current_storage;
	od_cfg_balancing_t *current_balancing;
	od_cfg_balancing_method_t *current_balancing_method;
	od_cfg_soft_oom_t *current_soft_oom;
	od_cfg_soft_oom_drop_t *current_soft_oom_drop;
	od_cfg_ldap_endpoint_t *current_ldap_endpoint;
	od_cfg_database_t *current_database;
	od_cfg_route_t *current_user;
	od_cfg_route_t *current_watchdog;
	od_cfg_route_t *current_group;
	od_cfg_string_kvp_list_t *current_kvp_list;
	od_cfg_ldap_storage_credentials_t *current_ldap_creds;
} od_cfg_parse_ctx_t;

void od_cfg_parse_ctx_init(od_cfg_parse_ctx_t *ctx, const char *filename,
			   char *input, size_t input_size,
			   od_cfg_model_t *model, od_cfg_diag_list_t *diags);

void od_cfg_parse_ctx_free(od_cfg_parse_ctx_t *ctx);

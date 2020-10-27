
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "console.h"

enum
{
	OD_LKILL_CLIENT,
	OD_LRELOAD,
	OD_LSHOW,
	OD_LSTATS,
	OD_LSERVERS,
	OD_LCLIENTS,
	OD_LLISTS,
	OD_LSET,
	OD_LCREATE,
	OD_LDROP,
	OD_LPOOLS,
	OD_LPOOLS_EXTENDED,
	OD_LDATABASES,
	OD_LMODULE,
	OD_LERRORS,
	OD_LERRORS_PER_ROUTE,
	OD_LFRONTEND,
	OD_LROUTER,
	OD_LVERSION,
};

static od_keyword_t od_console_keywords[] = {
	od_keyword("kill_client", OD_LKILL_CLIENT),
	od_keyword("reload", OD_LRELOAD),
	od_keyword("show", OD_LSHOW),
	od_keyword("stats", OD_LSTATS),
	od_keyword("servers", OD_LSERVERS),
	od_keyword("clients", OD_LCLIENTS),
	od_keyword("lists", OD_LLISTS),
	od_keyword("set", OD_LSET),
	od_keyword("pools", OD_LPOOLS),
	od_keyword("pools_extended", OD_LPOOLS_EXTENDED),
	od_keyword("databases", OD_LDATABASES),
	od_keyword("create", OD_LCREATE),
	od_keyword("module", OD_LMODULE),
	od_keyword("errors", OD_LERRORS),
	od_keyword("errors_per_route", OD_LERRORS_PER_ROUTE),
	od_keyword("frontend", OD_LFRONTEND),
	od_keyword("router", OD_LROUTER),
	od_keyword("drop", OD_LDROP),
	od_keyword("version", OD_LVERSION),
	{ 0, 0, 0 }
};

static inline int
od_console_show_stats_add(machine_msg_t *stream,
                          char *database,
                          int database_len,
                          od_stat_t *total,
                          od_stat_t *avg)
{
	assert(stream);
	int offset;
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL)
		return -1;
	int rc;
	rc = kiwi_be_write_data_row_add(stream, offset, database, database_len);
	if (rc == -1)
		return -1;
	char data[64];
	int data_len;
	/* total_xact_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->count_tx);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_query_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->count_query);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_received */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->recv_client);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_sent */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->recv_server);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_xact_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->tx_time);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_query_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->query_time);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_wait_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_xact_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->count_tx);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_query_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->count_query);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_recv */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->recv_client);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_sent */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->recv_server);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_xact_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->tx_time);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_query_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->query_time);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_wait_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static inline od_retcode_t
od_console_show_frontend_stats_err_add(machine_msg_t *stream,
                                       od_route_pool_t *route_pool)
{
	assert(stream);

	for (size_t i = 0; i < OD_FRONTEND_STATUS_ERRORS_TYPES_COUNT; ++i) {
		int offset;
		int rc;
		machine_msg_t *msg;

		msg = kiwi_be_write_data_row(stream, &offset);
		if (msg == NULL)
			return NOT_OK_RESPONSE;

		size_t total_count = od_err_logger_get_aggr_errors_count(
		  route_pool->err_logger, od_frontend_status_errs[i]);

		char *err_type = od_frontend_status_to_str(od_frontend_status_errs[i]);

		rc = kiwi_be_write_data_row_add(
		  stream, offset, err_type, strlen(err_type));
		if (rc != OK_RESPONSE) {
			return rc;
		}
		char data[64];
		int data_len;
		/* error_type */
		data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total_count);

		rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
		if (rc != OK_RESPONSE) {
			return rc;
		}
	}

	return OK_RESPONSE;
}

static inline int
od_console_show_router_stats_err_add(machine_msg_t *stream,
                                     od_error_logger_t *err_logger)
{
	assert(stream);

	for (size_t i = 0; i < OD_ROUTER_STATUS_ERRORS_TYPES_COUNT; ++i) {
		int offset;
		int rc;
		machine_msg_t *msg;

		msg = kiwi_be_write_data_row(stream, &offset);
		if (msg == NULL)
			return NOT_OK_RESPONSE;

		char *err_type = od_router_status_to_str(od_router_status_errs[i]);

		rc = kiwi_be_write_data_row_add(
		  stream, offset, err_type, strlen(err_type));
		if (rc != OK_RESPONSE) {
			return rc;
		}
		char data[64];
		int data_len;
		/* error_type */
		data_len = od_snprintf(data,
		                       sizeof(data),
		                       "%" PRIu64,
		                       od_err_logger_get_aggr_errors_count(
		                         err_logger, od_router_status_errs[i]));

		rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
		if (rc != OK_RESPONSE) {
			return rc;
		}
	}

	return OK_RESPONSE;
}

static int
od_console_show_stats_cb(char *database,
                         int database_len,
                         od_stat_t *total,
                         od_stat_t *avg,
                         void **argv)
{
	machine_msg_t *stream = argv[0];
	return od_console_show_stats_add(
	  stream, database, database_len, total, avg);
}

static int
od_console_show_err_frontend_stats_cb(od_route_pool_t *pool, void **argv)
{
	machine_msg_t *stream = argv[0];
	return od_console_show_frontend_stats_err_add(stream, pool);
}

static int
od_console_show_err_router_stats_cb(od_error_logger_t *l, void **argv)
{
	machine_msg_t *stream = argv[0];
	return od_console_show_router_stats_err_add(stream, l);
}

static inline int
od_console_show_stats(od_client_t *client, machine_msg_t *stream)
{
	assert(stream);
	od_router_t *router = client->global->router;
	od_cron_t *cron     = client->global->cron;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream,
	                                     "sllllllllllllll",
	                                     "database",
	                                     "total_xact_count",
	                                     "total_query_count",
	                                     "total_received",
	                                     "total_sent",
	                                     "total_xact_time",
	                                     "total_query_time",
	                                     "total_wait_time",
	                                     "avg_xact_count",
	                                     "avg_query_count",
	                                     "avg_recv",
	                                     "avg_sent",
	                                     "avg_xact_time",
	                                     "avg_query_time",
	                                     "avg_wait_time");
	if (msg == NULL)
		return -1;

	void *argv[] = { stream };
	od_route_pool_stat_database(
	  &router->route_pool, od_console_show_stats_cb, cron->stat_time_us, argv);

	int rc = kiwi_be_write_complete(stream, "SHOW", 5);
	if (rc == -1) {
		return rc;
	}

	return rc;
}

static inline od_retcode_t
od_console_show_errors(od_client_t *client, machine_msg_t *stream)
{
	assert(stream);
	od_router_t *router = client->global->router;

	void *argv[] = { stream };

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream, "sl", "error_type", "count");

	if (msg == NULL)
		return NOT_OK_RESPONSE;

	int rc;
	rc = od_route_pool_stat_err_router(
	  router, od_console_show_err_router_stats_cb, argv);

	if (rc != OK_RESPONSE)
		return rc;

	rc = od_route_pool_stat_err_frontend(
	  &router->route_pool, od_console_show_err_frontend_stats_cb, argv);

	if (rc != OK_RESPONSE)
		return rc;

	rc = kiwi_be_write_complete(stream, "SHOW", 5);
	return rc;
}

static inline int
od_console_show_errors_per_route_cb(od_route_t *route, void **argv)
{
	machine_msg_t *stream = argv[0];
	assert(stream);

	if (!route || !route->extra_logging_enabled || od_route_is_dynamic(route)) {
		return OK_RESPONSE;
	}
	for (size_t i = 0; i < OD_FRONTEND_STATUS_ERRORS_TYPES_COUNT; ++i) {
		int offset;
		int rc;
		machine_msg_t *msg;
		msg = kiwi_be_write_data_row(stream, &offset);
		if (msg == NULL)
			return NOT_OK_RESPONSE;

		size_t total_count = od_err_logger_get_aggr_errors_count(
		  route->err_logger, od_frontend_status_errs[i]);

		char *err_type = od_frontend_status_to_str(od_frontend_status_errs[i]);

		rc = kiwi_be_write_data_row_add(
		  stream, offset, err_type, strlen(err_type));
		if (rc != OK_RESPONSE) {
			return rc;
		}

		/* route user */

		rc = kiwi_be_write_data_row_add(stream,
		                                offset,
		                                route->rule->user_name,
		                                strlen(route->rule->user_name));
		if (rc != OK_RESPONSE) {
			return rc;
		}

		/* route database */

		rc = kiwi_be_write_data_row_add(
		  stream, offset, route->rule->db_name, strlen(route->rule->db_name));
		if (rc != OK_RESPONSE) {
			return rc;
		}

		/* error_type */

		char data[64];
		int data_len;
		data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total_count);

		rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
		if (rc != OK_RESPONSE) {
			return rc;
		}
	}

	for (size_t i = 0; i < OD_ROUTER_ROUTE_STATUS_ERRORS_TYPES_COUNT; ++i) {
		int offset;
		int rc;
		machine_msg_t *msg;
		msg = kiwi_be_write_data_row(stream, &offset);
		if (msg == NULL)
			return NOT_OK_RESPONSE;

		size_t total_count = od_err_logger_get_aggr_errors_count(
		  route->err_logger, od_router_route_status_errs[i]);

		char *err_type =
		  od_router_status_to_str(od_router_route_status_errs[i]);

		rc = kiwi_be_write_data_row_add(
		  stream, offset, err_type, strlen(err_type));
		if (rc != OK_RESPONSE) {
			return rc;
		}

		/* route user */

		rc = kiwi_be_write_data_row_add(stream,
		                                offset,
		                                route->rule->user_name,
		                                strlen(route->rule->user_name));
		if (rc != OK_RESPONSE) {
			return rc;
		}

		/* route database */

		rc = kiwi_be_write_data_row_add(
		  stream, offset, route->rule->db_name, strlen(route->rule->db_name));
		if (rc != OK_RESPONSE) {
			return rc;
		}

		/* error_type */

		char data[64];
		int data_len;
		data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total_count);

		rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
		if (rc != OK_RESPONSE) {
			return rc;
		}
	}

	return OK_RESPONSE;
}

static inline od_retcode_t
od_console_show_errors_per_route(od_client_t *client, machine_msg_t *stream)
{

	assert(stream);
	od_router_t *router = client->global->router;

	void *argv[] = { stream };

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(
	  stream, "sssl", "error_type", "user", "database", "count");

	if (msg == NULL) {
		return NOT_OK_RESPONSE;
	}

	od_router_foreach(router, od_console_show_errors_per_route_cb, argv);

	od_retcode_t rc = kiwi_be_write_complete(stream, "SHOW", 5);
	return rc;
}

static inline int
od_console_show_version(machine_msg_t *stream)
{
	assert(stream);

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream, "s", "version");

	if (msg == NULL)
		return NOT_OK_RESPONSE;
	int offset;
	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL)
		return NOT_OK_RESPONSE;

	char data[128];
	int data_len;
	/* current version and build */
	data_len = od_snprintf(data,
	                       sizeof(data),
	                       "%s-%s-%s",
	                       OD_VERSION_NUMBER,
	                       OD_VERSION_GIT,
	                       OD_VERSION_BUILD);

	int rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc != OK_RESPONSE)
		return rc;

	rc = kiwi_be_write_complete(stream, "SHOW", 5);
	return rc;
}

static inline int
od_console_show_pools_add_cb(od_route_t *route, void **argv)
{
	int offset;
	machine_msg_t *stream = argv[0];
	bool *extended        = argv[1];
	double *quantiles     = argv[2];
	int *quantiles_count  = argv[3];
	machine_msg_t *msg;
	td_histogram_t *transactions_hgram = NULL;
	td_histogram_t *queries_hgram      = NULL;
	td_histogram_t *freeze_hgram       = NULL;
	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL)
		return -1;

	od_route_lock(route);
	int rc;
	rc = kiwi_be_write_data_row_add(
	  stream, offset, route->id.database, route->id.database_len - 1);
	if (rc == -1)
		goto error;
	rc = kiwi_be_write_data_row_add(
	  stream, offset, route->id.user, route->id.user_len - 1);
	if (rc == -1)
		goto error;
	char data[64];
	int data_len;

	/* cl_active */
	data_len =
	  od_snprintf(data, sizeof(data), "%d", route->client_pool.count_active);
	rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* cl_waiting */
	data_len =
	  od_snprintf(data, sizeof(data), "%d", route->client_pool.count_pending);
	rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* sv_active */
	data_len =
	  od_snprintf(data, sizeof(data), "%d", route->server_pool.count_active);
	rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* sv_idle */
	data_len =
	  od_snprintf(data, sizeof(data), "%d", route->server_pool.count_idle);
	rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* sv_used */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* sv_tested */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* sv_login */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* maxwait */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;
	/* maxwait_us */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	/* pool_mode */
	rc = -1;
	if (route->rule->pool == OD_RULE_POOL_SESSION)
		rc = kiwi_be_write_data_row_add(stream, offset, "session", 7);
	if (route->rule->pool == OD_RULE_POOL_TRANSACTION)
		rc = kiwi_be_write_data_row_add(stream, offset, "transaction", 11);
	if (rc == -1)
		goto error;

	if (*extended) {
		/* bytes recived */
		data_len =
		  od_snprintf(data, sizeof(data), "%" PRIu64, route->stats.recv_client);
		rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
		if (rc == -1)
			goto error;
		/* bytes sent */
		data_len =
		  od_snprintf(data, sizeof(data), "%" PRIu64, route->stats.recv_server);
		rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
		if (rc == -1)
			goto error;

		/* tcp conn rate */
		data_len =
		  od_snprintf(data, sizeof(data), "%" PRIu64, route->tcp_connections);
		rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
		if (rc == -1)
			goto error;

		transactions_hgram = td_new(QUANTILES_COMPRESSION);
		queries_hgram      = td_new(QUANTILES_COMPRESSION);
		freeze_hgram       = td_new(QUANTILES_COMPRESSION);
		if (route->stats.enable_quantiles) {
			for (size_t i = 0; i < QUANTILES_WINDOW; ++i) {
				td_copy(freeze_hgram, route->stats.transaction_hgram[i]);
				td_merge(transactions_hgram, freeze_hgram);
				td_copy(freeze_hgram, route->stats.query_hgram[i]);
				td_merge(queries_hgram, freeze_hgram);
			}
		}
		for (int i = 0; i < *quantiles_count; i++) {
			double q = quantiles[i];
			/* query quantile */
			double query_quantile       = td_value_at(queries_hgram, q);
			double transaction_quantile = td_value_at(transactions_hgram, q);
			if (isnan(query_quantile)) {
				query_quantile = 0;
			}
			if (isnan(transaction_quantile)) {
				transaction_quantile = 0;
			}
			data_len = od_snprintf(
			  data, sizeof(data), "%" PRIu64, (uint64_t)query_quantile);
			rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
			if (rc == -1)
				goto error;
			/* transaction quantile */
			data_len = od_snprintf(
			  data, sizeof(data), "%" PRIu64, (uint64_t)transaction_quantile);
			rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
			if (rc == -1)
				goto error;
		}
	}
	td_safe_free(transactions_hgram);
	td_safe_free(queries_hgram);
	td_safe_free(freeze_hgram);
	od_route_unlock(route);
	return 0;
error:
	td_safe_free(transactions_hgram);
	td_safe_free(queries_hgram);
	td_safe_free(freeze_hgram);
	od_route_unlock(route);
	return -1;
}

static inline int
od_console_show_databases_add_cb(od_route_t *route, void **argv)
{
	int offset;
	machine_msg_t *stream = argv[0];
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL)
		return -1;

	od_route_lock(route);
	int rc;
	rc = kiwi_be_write_data_row_add(
	  stream, offset, route->id.database, route->id.database_len - 1);
	if (rc == -1)
		goto error;
	od_rule_t *rule            = route->rule;
	od_rule_storage_t *storage = rule->storage;

	char *host = storage->host;
	if (!host)
		host = "";
	rc = kiwi_be_write_data_row_add(stream, offset, host, strlen(host));
	if (rc == -1)
		goto error;
	char data[64];
	int data_len;

	/* port */
	data_len = od_snprintf(data, sizeof(data), "%d", storage->port);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	/* database */
	rc = kiwi_be_write_data_row_add(
	  stream, offset, rule->db_name, rule->db_name_len);
	if (rc == -1)
		goto error;

	/* force_user */
	rc = kiwi_be_write_data_row_add(stream, offset, "", 0);
	if (rc == -1)
		goto error;

	/* pool_size */
	data_len = od_snprintf(data, sizeof(data), "%d", rule->pool_size);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	/* reserve_pool */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	/* pool_mode */
	rc = -1;
	if (rule->pool == OD_RULE_POOL_SESSION)
		rc = kiwi_be_write_data_row_add(stream, offset, "session", 7);
	if (rule->pool == OD_RULE_POOL_TRANSACTION)
		rc = kiwi_be_write_data_row_add(stream, offset, "transaction", 11);
	if (rc == -1)
		goto error;

	/* max_connections */
	data_len = od_snprintf(data, sizeof(data), "%d", rule->client_max);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	/* current_connections */
	data_len = od_snprintf(data,
	                       sizeof(data),
	                       "%d",
	                       route->client_pool.count_active +
	                         route->client_pool.count_pending +
	                         route->client_pool.count_queue);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	/* paused */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	/* disabled */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0UL);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		goto error;

	od_route_unlock(route);
	return 0;
error:
	od_route_unlock(route);
	return -1;
}

static inline int
od_console_show_databases(od_client_t *client, machine_msg_t *stream)
{
	assert(stream);
	od_router_t *router = client->global->router;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream,
	                                     "sslssllsllll",
	                                     "name",
	                                     "host",
	                                     "port",
	                                     "database",
	                                     "force_user",
	                                     "pool_size",
	                                     "reserve_pool",
	                                     "pool_mode",
	                                     "max_connections",
	                                     "current_connections",
	                                     "paused",
	                                     "disabled");
	if (msg == NULL)
		return -1;

	void *argv[] = { stream };
	int rc;
	rc = od_router_foreach(router, od_console_show_databases_add_cb, argv);
	if (rc == -1)
		return -1;

	return kiwi_be_write_complete(stream, "SHOW", 5);
}

static inline int
od_console_show_pools(od_client_t *client, machine_msg_t *stream, bool extended)
{
	assert(stream);
	int rc;
	od_router_t *router = client->global->router;
	od_route_t *route   = client->route;
	double *quantiles   = route->rule->quantiles;
	int quantiles_count = route->rule->quantiles_count;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream,
	                                     "ssllllllllls",
	                                     "database",
	                                     "user",
	                                     "cl_active",
	                                     "cl_waiting",
	                                     "sv_active",
	                                     "sv_idle",
	                                     "sv_used",
	                                     "sv_tested",
	                                     "sv_login",
	                                     "maxwait",
	                                     "maxwait_us",
	                                     "pool_mode");
	if (msg == NULL)
		return -1;

	if (extended) {
		char *bytes_rcv = "bytes_recieved";
		rc              = kiwi_be_write_row_description_add(msg,
                                               0,
                                               bytes_rcv,
                                               strlen(bytes_rcv),
                                               0,
                                               0,
                                               23 /* INT4OID */,
                                               4,
                                               0,
                                               0);
		if (rc == -1)
			return -1;
		char *bytes_sent = "bytes_sent";
		rc               = kiwi_be_write_row_description_add(msg,
                                               0,
                                               bytes_sent,
                                               strlen(bytes_sent),
                                               0,
                                               0,
                                               23 /* INT4OID */,
                                               4,
                                               0,
                                               0);
		if (rc == -1)
			return -1;

		char *tcp_conn_rate = "tcp_conn_count";
		rc                  = kiwi_be_write_row_description_add(msg,
                                               0,
                                               tcp_conn_rate,
                                               strlen(tcp_conn_rate),
                                               0,
                                               0,
                                               23 /* INT4OID */,
                                               4,
                                               0,
                                               0);
		if (rc == -1)
			return -1;

		for (int i = 0; i < quantiles_count; i++) {
			char caption[KIWI_MAX_VAR_SIZE];
			int caption_len;
			caption_len =
			  od_snprintf(caption, sizeof(caption), "query_%.6g", quantiles[i]);
			rc = kiwi_be_write_row_description_add(
			  msg, 0, caption, caption_len, 0, 0, 23 /* INT4OID */, 4, 0, 0);
			if (rc == -1)
				return -1;
			caption_len = od_snprintf(
			  caption, sizeof(caption), "transaction_%.6g", quantiles[i]);
			rc = kiwi_be_write_row_description_add(
			  msg, 0, caption, caption_len, 0, 0, 23 /* INT4OID */, 4, 0, 0);
			if (rc == -1)
				return -1;
		}
	}

	void *argv[] = { stream, &extended, quantiles, &quantiles_count };
	rc = od_router_foreach(router, od_console_show_pools_add_cb, argv);
	if (rc == -1)
		return -1;

	return kiwi_be_write_complete(stream, "SHOW", 5);
}

static inline int
od_console_show_servers_server_cb(od_server_t *server, void **argv)
{
	od_route_t *route = server->route;

	int offset;
	machine_msg_t *stream = argv[0];
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL)
		return -1;
	/* type */
	char data[64];
	size_t data_len;
	data_len = od_snprintf(data, sizeof(data), "S");
	int rc;
	rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* user */
	rc = kiwi_be_write_data_row_add(
	  stream, offset, route->id.user, route->id.user_len - 1);
	if (rc == -1)
		return -1;
	/* database */
	rc = kiwi_be_write_data_row_add(
	  stream, offset, route->id.database, route->id.database_len - 1);
	if (rc == -1)
		return -1;
	/* state */
	char *state = "";
	if (server->state == OD_SERVER_IDLE)
		state = "idle";
	else if (server->state == OD_SERVER_ACTIVE)
		state = "active";
	rc = kiwi_be_write_data_row_add(stream, offset, state, strlen(state));
	if (rc == -1)
		return -1;
	/* addr */
	od_getpeername(server->io.io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* port */
	od_getpeername(server->io.io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_addr */
	od_getsockname(server->io.io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_port */
	od_getsockname(server->io.io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* connect_time */
	rc = kiwi_be_write_data_row_add(msg, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* request_time */
	rc = kiwi_be_write_data_row_add(msg, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* wait */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* wait_us */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* ptr */
	data_len = od_snprintf(data,
	                       sizeof(data),
	                       "%s%.*s",
	                       server->id.id_prefix,
	                       (signed)sizeof(server->id.id),
	                       server->id.id);
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* link */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* remote_pid */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* tls */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc       = kiwi_be_write_data_row_add(msg, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_servers_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	od_server_pool_foreach(&route->server_pool,
	                       OD_SERVER_ACTIVE,
	                       od_console_show_servers_server_cb,
	                       argv);

	od_server_pool_foreach(&route->server_pool,
	                       OD_SERVER_IDLE,
	                       od_console_show_servers_server_cb,
	                       argv);

	od_route_unlock(route);
	return 0;
}

static inline int
od_console_show_servers(od_client_t *client, machine_msg_t *stream)
{
	assert(stream);
	od_router_t *router = client->global->router;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream,
	                                     "sssssdsdssddssds",
	                                     "type",
	                                     "user",
	                                     "database",
	                                     "state",
	                                     "addr",
	                                     "port",
	                                     "local_addr",
	                                     "local_port",
	                                     "connect_time",
	                                     "request_time",
	                                     "wait",
	                                     "wait_us",
	                                     "ptr",
	                                     "link",
	                                     "remote_pid",
	                                     "tls");
	if (msg == NULL)
		return -1;

	void *argv[] = { stream };
	od_router_foreach(router, od_console_show_servers_cb, argv);

	return kiwi_be_write_complete(stream, "SHOW", 5);
}

static inline int
od_console_show_clients_callback(od_client_t *client, void **argv)
{
	int offset;
	machine_msg_t *stream = argv[0];
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL)
		return -1;
	char data[64];
	size_t data_len;
	/* type */
	data_len = od_snprintf(data, sizeof(data), "C");
	int rc;
	rc = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* user */
	rc = kiwi_be_write_data_row_add(stream,
	                                offset,
	                                client->startup.user.value,
	                                client->startup.user.value_len - 1);
	if (rc == -1)
		return -1;
	/* database */
	rc = kiwi_be_write_data_row_add(stream,
	                                offset,
	                                client->startup.database.value,
	                                client->startup.database.value_len - 1);
	if (rc == -1)
		return -1;
	/* state */
	char *state = "";
	if (client->state == OD_CLIENT_ACTIVE)
		state = "active";
	else if (client->state == OD_CLIENT_PENDING)
		state = "pending";
	else if (client->state == OD_CLIENT_QUEUE)
		state = "queue";
	rc = kiwi_be_write_data_row_add(stream, offset, state, strlen(state));
	if (rc == -1)
		return -1;
	/* addr */
	od_getpeername(client->io.io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* port */
	od_getpeername(client->io.io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_addr */
	od_getsockname(client->io.io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_port */
	od_getsockname(client->io.io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* connect_time */
	rc = kiwi_be_write_data_row_add(stream, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* request_time */
	rc = kiwi_be_write_data_row_add(stream, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* wait */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* wait_us */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* ptr */
	data_len = od_snprintf(data,
	                       sizeof(data),
	                       "%s%.*s",
	                       client->id.id_prefix,
	                       (signed)sizeof(client->id.id),
	                       client->id.id);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* link */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* remote_pid */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* tls */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_clients_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	od_client_pool_foreach(&route->client_pool,
	                       OD_CLIENT_ACTIVE,
	                       od_console_show_clients_callback,
	                       argv);

	od_client_pool_foreach(&route->client_pool,
	                       OD_CLIENT_PENDING,
	                       od_console_show_clients_callback,
	                       argv);

	od_client_pool_foreach(&route->client_pool,
	                       OD_CLIENT_QUEUE,
	                       od_console_show_clients_callback,
	                       argv);

	od_route_unlock(route);
	return 0;
}

static inline int
od_console_show_clients(od_client_t *client, machine_msg_t *stream)
{
	assert(stream);
	od_router_t *router = client->global->router;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream,
	                                     "sssssdsdssddssds",
	                                     "type",
	                                     "user",
	                                     "database",
	                                     "state",
	                                     "addr",
	                                     "port",
	                                     "local_addr",
	                                     "local_port",
	                                     "connect_time",
	                                     "request_time",
	                                     "wait",
	                                     "wait_us",
	                                     "ptr",
	                                     "link",
	                                     "remote_pid",
	                                     "tls");
	if (msg == NULL)
		return -1;

	void *argv[] = { stream };
	od_router_foreach(router, od_console_show_clients_cb, argv);

	return kiwi_be_write_complete(stream, "SHOW", 5);
}

static inline int
od_console_show_lists_add(machine_msg_t *stream, char *list, int items)
{
	int offset;
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL)
		return -1;
	/* list */
	int rc;
	rc = kiwi_be_write_data_row_add(stream, offset, list, strlen(list));
	if (rc == -1)
		return -1;
	/* items */
	char data[64];
	int data_len;
	data_len = od_snprintf(data, sizeof(data), "%d", items);
	rc       = kiwi_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_lists_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	int *used_servers = argv[0];
	int *free_servers = argv[1];
	(*used_servers) += route->server_pool.count_active;
	(*free_servers) += route->server_pool.count_idle;

	od_route_unlock(route);
	return 0;
}

static inline int
od_console_show_lists(od_client_t *client, machine_msg_t *stream)
{
	assert(stream);
	od_router_t *router = client->global->router;

	/* Gather router information.

	   router_used_servers can be inconsistent here, since it depends on
	   separate route locks.
	*/
	od_router_lock(router);

	int router_used_servers = 0;
	int router_free_servers = 0;
	int router_pools        = router->route_pool.count;
	int router_clients      = od_atomic_u32_of(&router->clients);

	void *argv[] = { &router_used_servers, &router_free_servers };
	od_route_pool_foreach(&router->route_pool, od_console_show_lists_cb, argv);

	od_router_unlock(router);

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf(stream, "sd", "list", "items");
	if (msg == NULL)
		return -1;
	int rc;
	/* databases */
	rc = od_console_show_lists_add(stream, "databases", 0);
	if (rc == -1)
		return -1;
	/* users */
	rc = od_console_show_lists_add(stream, "users", 0);
	if (rc == -1)
		return -1;
	/* pools */
	rc = od_console_show_lists_add(stream, "pools", router_pools);
	if (rc == -1)
		return -1;
	/* free_clients */
	rc = od_console_show_lists_add(stream, "free_clients", 0);
	if (rc == -1)
		return -1;
	/* used_clients */
	rc = od_console_show_lists_add(stream, "used_clients", router_clients);
	if (rc == -1)
		return -1;
	/* login_clients */
	rc = od_console_show_lists_add(stream, "login_clients", 0);
	if (rc == -1)
		return -1;
	/* free_servers */
	rc = od_console_show_lists_add(stream, "free_servers", router_free_servers);
	if (rc == -1)
		return -1;
	/* used_servers */
	rc = od_console_show_lists_add(stream, "used_servers", router_used_servers);
	if (rc == -1)
		return -1;
	/* dns_names */
	rc = od_console_show_lists_add(stream, "dns_names", 0);
	if (rc == -1)
		return -1;
	/* dns_zones */
	rc = od_console_show_lists_add(stream, "dns_zones", 0);
	if (rc == -1)
		return -1;
	/* dns_queries */
	rc = od_console_show_lists_add(stream, "dns_queries", 0);
	if (rc == -1)
		return -1;
	/* dns_pending */
	rc = od_console_show_lists_add(stream, "dns_pending", 0);
	if (rc == -1)
		return -1;
	return kiwi_be_write_complete(stream, "SHOW", 5);
}

static inline int
od_console_show(od_client_t *client, machine_msg_t *stream, od_parser_t *parser)
{
	assert(stream);
	od_token_t token;
	int rc;
	rc = od_parser_next(parser, &token);
	switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
		default:
			return -1;
	}
	od_keyword_t *keyword;
	keyword = od_keyword_match(od_console_keywords, &token);
	if (keyword == NULL)
		return -1;
	switch (keyword->id) {
		case OD_LSTATS:
			return od_console_show_stats(client, stream);
		case OD_LPOOLS:
			return od_console_show_pools(client, stream, false);
		case OD_LPOOLS_EXTENDED:
			return od_console_show_pools(client, stream, true);
		case OD_LDATABASES:
			return od_console_show_databases(client, stream);
		case OD_LSERVERS:
			return od_console_show_servers(client, stream);
		case OD_LCLIENTS:
			return od_console_show_clients(client, stream);
		case OD_LLISTS:
			return od_console_show_lists(client, stream);
		case OD_LERRORS:
			return od_console_show_errors(client, stream);
		case OD_LERRORS_PER_ROUTE:
			return od_console_show_errors_per_route(client, stream);
		case OD_LVERSION:
			return od_console_show_version(stream);
	}
	return -1;
}

static inline int
od_console_kill_client(od_client_t *client,
                       machine_msg_t *stream,
                       od_parser_t *parser)
{
	(void)stream;
	od_token_t token;
	int rc;
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_KEYWORD)
		return -1;
	od_id_t id;
	if (token.value.string.size != (sizeof(id.id) + 1))
		return -1;
	memcpy(id.id, token.value.string.pointer + 1, sizeof(id.id));

	od_router_kill(client->global->router, &id);
	return 0;
}

static inline int
od_console_reload(od_client_t *client, machine_msg_t *stream)
{
	od_instance_t *instance = client->global->instance;

	od_log(&instance->logger, "console", NULL, NULL, "RELOAD command received");
	od_system_config_reload(client->global->system);
	return kiwi_be_write_complete(stream, "RELOAD", 7);
}

static inline int
od_console_set(od_client_t *client, machine_msg_t *stream)
{
	(void)client;
	/* reply success */
	return kiwi_be_write_complete(stream, "SET", 4);
}

static inline int
od_console_add_module(od_client_t *client,
                      machine_msg_t *stream,
                      od_parser_t *parser)
{
	assert(stream);
	od_token_t token;
	int rc;
	rc                      = od_parser_next(parser, &token);
	od_instance_t *instance = client->global->instance;

	switch (rc) {
		case OD_PARSER_STRING: {
			char module_path[MAX_MODULE_PATH_LEN];
			od_token_to_string_dest(&token, module_path);

			od_log(&instance->logger,
			       "od module dynamic load",
			       NULL,
			       NULL,
			       "loading module with path %s",
			       module_path);
			int retcode = od_target_module_add(
			  &instance->logger, client->global->modules, module_path);
			if (retcode == 0) {
				od_frontend_infof(
				  client, stream, "module was successfully loaded!");
			} else {
				od_frontend_errorf(
				  client,
				  stream,
				  KIWI_SYSTEM_ERROR,
				  "module was NOT successfully loaded! Check logs for details");
			}
			return retcode;
		}
		case OD_PARSER_EOF:
		default:
			return -1;
	}
}

static inline int
od_console_unload_module(od_client_t *client,
                         machine_msg_t *stream,
                         od_parser_t *parser)
{
	assert(stream);
	od_token_t token;
	int rc;
	rc                      = od_parser_next(parser, &token);
	od_instance_t *instance = client->global->instance;

	switch (rc) {
		case OD_PARSER_STRING: {
			char module_path[MAX_MODULE_PATH_LEN];
			od_token_to_string_dest(&token, module_path);

			od_log(&instance->logger,
			       "od module dynamic unload",
			       NULL,
			       NULL,
			       "unloading module with path %s",
			       module_path);
			int retcode = od_target_module_unload(
			  &instance->logger, client->global->modules, module_path);
			if (retcode == 0) {
				od_frontend_infof(
				  client, stream, "module was successfully unloaded!");
			} else {
				od_frontend_errorf(client,
				                   stream,
				                   KIWI_SYSTEM_ERROR,
				                   "module was NOT successfully "
				                   "unloaded! Check logs for details");
			}
			return retcode;
		}
		case OD_PARSER_EOF:
		default:
			return -1;
	}
}

static inline int
od_console_create(od_client_t *client,
                  machine_msg_t *stream,
                  od_parser_t *parser)
{
	assert(stream);
	od_token_t token;
	int rc;
	rc = od_parser_next(parser, &token);
	switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
		default:
			return -1;
	}
	od_keyword_t *keyword;
	keyword = od_keyword_match(od_console_keywords, &token);
	if (keyword == NULL)
		return -1;

	switch (keyword->id) {
		case OD_LMODULE:
			return od_console_add_module(client, stream, parser);
	}

	return -1;
}

static inline int
od_console_drop(od_client_t *client, machine_msg_t *stream, od_parser_t *parser)
{
	assert(stream);
	od_token_t token;
	int rc;
	rc = od_parser_next(parser, &token);
	switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
		default:
			return -1;
	}
	od_keyword_t *keyword;
	keyword = od_keyword_match(od_console_keywords, &token);
	if (keyword == NULL)
		return -1;

	switch (keyword->id) {
		case OD_LMODULE:
			return od_console_unload_module(client, stream, parser);
	}

	return -1;
}

int
od_console_query(od_client_t *client,
                 machine_msg_t *stream,
                 char *query_data,
                 uint32_t query_data_size)
{
	od_instance_t *instance = client->global->instance;

	uint32_t query_len;
	char *query;
	machine_msg_t *msg;
	int rc;
	rc = kiwi_be_read_query(query_data, query_data_size, &query, &query_len);
	if (rc == -1) {
		od_error(
		  &instance->logger, "console", client, NULL, "bad console command");
		msg = od_frontend_errorf(
		  client, stream, KIWI_SYNTAX_ERROR, "bad console command");
		if (msg == NULL)
			return -1;

		return 0;
	}

	if (instance->config.log_query)
		od_debug(
		  &instance->logger, "console", client, NULL, "%.*s", query_len, query);

	od_parser_t parser;
	od_parser_init(&parser, query, query_len);

	od_token_t token;
	rc = od_parser_next(&parser, &token);
	switch (rc) {
		case OD_PARSER_KEYWORD:
			break;
		case OD_PARSER_EOF:
		default:
			goto bad_query;
	}
	od_keyword_t *keyword;
	keyword = od_keyword_match(od_console_keywords, &token);
	if (keyword == NULL)
		goto bad_query;
	switch (keyword->id) {
		case OD_LSHOW:
			rc = od_console_show(client, stream, &parser);
			if (rc == -1)
				goto bad_query;
			break;
		case OD_LKILL_CLIENT:
			rc = od_console_kill_client(client, stream, &parser);
			if (rc == -1)
				goto bad_query;
			break;
		case OD_LRELOAD:
			rc = od_console_reload(client, stream);
			if (rc == -1)
				goto bad_query;
			break;
		case OD_LSET:
			rc = od_console_set(client, stream);
			if (rc == -1)
				goto bad_query;
			break;
		case OD_LCREATE:
			rc = od_console_create(client, stream, &parser);
			if (rc == -1) {
				goto bad_query;
			}
			break;
		case OD_LDROP:
			rc = od_console_drop(client, stream, &parser);
			if (rc == -1) {
				goto bad_query;
			}
			break;
		default:
			goto bad_query;
	}

	return 0;

bad_query:
	od_error(&instance->logger,
	         "console",
	         client,
	         NULL,
	         "console command error: %.*s",
	         query_len,
	         query);

	msg = od_frontend_errorf(client,
	                         stream,
	                         KIWI_SYNTAX_ERROR,
	                         "console command error: %.*s",
	                         query_len,
	                         query);
	if (msg == NULL)
		return -1;

	return 0;
}

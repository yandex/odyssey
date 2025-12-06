#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_compression_frontend_setup(od_client_t *, od_config_listen_t *,
				  od_logger_t *);

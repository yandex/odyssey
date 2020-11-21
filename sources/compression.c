
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */
#include <string.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

int
od_compression_frontend_setup(od_client_t *client, od_logger_t *logger)
{
	kiwi_var_t *compression_var =
	  kiwi_vars_get(&client->vars, KIWI_VAR_COMPRESSION);

	if (compression_var == NULL) {
		return 0;
	}

	char *client_compression_algorithms = compression_var->value;

	char compression_algorithm =
	  machine_compression_choose_alg(client_compression_algorithms);

	machine_msg_t *msg =
	  kiwi_be_write_compression_ack(NULL, compression_algorithm);

	if (msg == NULL)
		return -1;

	int rc = od_write(&client->io, msg);
	if (rc == -1) {
		od_error(logger,
		         "compression",
		         client,
		         NULL,
		         "write error: %s",
		         od_io_error(&client->io));
		return -1;
	}

	/* initialize compression */
	rc = machine_set_compression(client->io.io, compression_algorithm);
	if (rc == -1) {
		od_debug(logger,
		         "compression",
		         client,
		         NULL,
		         "unsupported compression algorithm");
		return -1;
	}

	return 0;
}

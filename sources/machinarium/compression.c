/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <string.h>

#include <machinarium/machinarium.h>

MACHINE_API
char machine_compression_choose_alg(char *client_compression_algorithms)
{
	/* chosen compression algorithm */
	char compression_algorithm = MM_ZPQ_NO_COMPRESSION;

	/* machinarium supported libpq compression algorithms */
	char server_compression_algorithms[MM_ZPQ_MAX_ALGORITHMS];

	/* get list of compression algorithms supported by machinarium */
	mm_zpq_get_supported_algorithms(server_compression_algorithms);

	/* intersect lists */
	while (*client_compression_algorithms != '\0') {
		if (strchr(server_compression_algorithms,
			   *client_compression_algorithms)) {
			compression_algorithm = *client_compression_algorithms;
			break;
		}
		client_compression_algorithms += 1;
	}

	return compression_algorithm;
}

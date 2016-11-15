
/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <so_macro.h>
#include <so_stream.h>
#include <so_header.h>
#include <so_feread.h>

int so_feread_ready(uint8_t *data, uint32_t size, int *status)
{
	if (so_unlikely(size != 1))
		return -1;
	*status = data[0];
	return 0;
}

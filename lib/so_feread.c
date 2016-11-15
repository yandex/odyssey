
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
#include <so_read.h>
#include <so_feread.h>

int so_feread_ready(uint8_t *data, uint32_t size, int *status)
{
	soheader_t *header = (soheader_t*)data;
	uint32_t len;
	int rc = so_read(&len, &data, &size);
	if (rc != 0)
		return -1;
	if (len != 1)
		return -1;
	*status = header->data[0];
	return 0;
}

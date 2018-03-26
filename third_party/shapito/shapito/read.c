
/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#include "shapito.h"

int shapito_read_startup(uint32_t *len, char **data, uint32_t *size)
{
	if (*size < sizeof(uint32_t))
		return sizeof(uint32_t) - *size;
	/* len */
	uint32_t pos_size = *size;
	char *pos = *data;
	shapito_stream_read32(len, &pos, &pos_size);
	uint32_t len_to_read;
	len_to_read = *len - *size;
	if (len_to_read > 0)
		return len_to_read;
	/* advance data stream */
	*data += *len;
	*size -= *len;
	/* set actual data size */
	*len  -= sizeof(uint32_t);
	return 0;
}

int shapito_read(uint32_t *len, char **data, uint32_t *size)
{
	if (*size < sizeof(shapito_header_t))
		return sizeof(shapito_header_t) - *size;
	uint32_t pos_size = *size - sizeof(uint8_t);
	char *pos = *data + sizeof(uint8_t);
	/* type */
	shapito_stream_read32(len, &pos, &pos_size);
	uint32_t len_to_read;
	len_to_read = (*len + sizeof(char)) - *size;
	if (len_to_read > 0)
		return len_to_read;
	/* advance data stream */
	*data += sizeof(uint8_t) + *len;
	*size -= sizeof(uint8_t) + *len;
	/* set actual data size */
	*len  -= sizeof(uint32_t);
	return 0;
}

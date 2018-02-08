#ifndef SHAPITO_BUF_H
#define SHAPITO_BUF_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_stream shapito_stream_t;

struct shapito_stream
{
	char             *start;
	char             *pos;
	char             *end;
	shapito_stream_t *next;
};

static inline void
shapito_stream_init(shapito_stream_t *stream)
{
	stream->start = NULL;
	stream->pos   = NULL;
	stream->end   = NULL;
	stream->next  = NULL;
}

static inline void
shapito_stream_free(shapito_stream_t *stream)
{
	if (stream->start == NULL)
		return;
	free(stream->start);
	stream->start = NULL;
	stream->pos   = NULL;
	stream->end   = NULL;
	stream->next  = NULL;
}

static inline int
shapito_stream_size(shapito_stream_t *stream)
{
	return stream->end - stream->start;
}

static inline int
shapito_stream_used(shapito_stream_t *stream)
{
	return stream->pos - stream->start;
}

static inline int
shapito_stream_left(shapito_stream_t *stream)
{
	return stream->end - stream->pos;
}

static inline void
shapito_stream_reset(shapito_stream_t *stream)
{
	stream->pos = stream->start;
}

static inline int
shapito_stream_ensure(shapito_stream_t *stream, int size)
{
	if (stream->end - stream->pos >= size)
		return 0;
	int sz = shapito_stream_size(stream) * 2;
	int actual = shapito_stream_used(stream) + size;
	if (actual > sz)
		sz = actual;
	char *pointer = realloc(stream->start, sz);
	if (pointer == NULL)
		return -1;
	stream->pos = pointer + (stream->pos - stream->start);
	stream->end = pointer + sz;
	stream->start = pointer;
	assert((stream->end - stream->pos) >= size);
	return 0;
}

static inline void
shapito_stream_advance(shapito_stream_t *stream, int size)
{
	stream->pos += size;
	assert(stream->pos <= stream->end);
}

static inline void
shapito_stream_write8to(char *dest, uint8_t value)
{
	*dest = (char)value;
}

static inline void
shapito_stream_write16to(char *dest, uint16_t value)
{
	dest[0] = (value >> 8) & 255;
	dest[1] =  value       & 255;
}

static inline void
shapito_stream_write32to(char *dest, uint32_t value)
{
	dest[0] = (value >> 24) & 255;
	dest[1] = (value >> 16) & 255;
	dest[2] = (value >> 8)  & 255;
	dest[3] =  value        & 255;
}

static inline void
shapito_stream_write8(shapito_stream_t *stream, uint8_t value)
{
	shapito_stream_write8to(stream->pos, value);
	shapito_stream_advance(stream, sizeof(uint8_t));
}

static inline void
shapito_stream_write16(shapito_stream_t *stream, uint16_t value)
{
	shapito_stream_write16to(stream->pos, value);
	shapito_stream_advance(stream, sizeof(uint16_t));
}

static inline void
shapito_stream_write32(shapito_stream_t *stream, uint32_t value)
{
	shapito_stream_write32to(stream->pos, value);
	shapito_stream_advance(stream, sizeof(uint32_t));
}

static inline void
shapito_stream_write(shapito_stream_t *stream, char *buf, int size)
{
	memcpy(stream->pos, buf, size);
	shapito_stream_advance(stream, size);
}

static inline int
shapito_stream_read8(char *out, char **pos, uint32_t *size)
{
	if (shapito_unlikely(*size < sizeof(uint8_t)))
		return -1;
	*out = *pos[0];
	*size -= sizeof(uint8_t);
	*pos  += sizeof(uint8_t);
	return 0;
}

static inline int
shapito_stream_read16(uint16_t *out, char **pos, uint32_t *size)
{
	if (shapito_unlikely(*size < sizeof(uint16_t)))
		return -1;
	unsigned char *ptr = (unsigned char*)*pos;
	*out = ptr[0] << 8 | ptr[1];
	*size -= sizeof(uint16_t);
	*pos  += sizeof(uint16_t);
	return 0;
}

static inline int
shapito_stream_read32(uint32_t *out, char **pos, uint32_t *size)
{
	if (shapito_unlikely(*size < sizeof(uint32_t)))
		return -1;
	unsigned char *ptr = (unsigned char*)*pos;
	*out = ptr[0] << 24 | ptr[1] << 16 |
	       ptr[2] <<  8 | ptr[3];
	*size -= sizeof(uint32_t);
	*pos  += sizeof(uint32_t);
	return 0;
}

static inline int
shapito_stream_readsz(char **pos, uint32_t *size)
{
	char *p = *pos;
	char *end = p + *size;
	while (p < end && *p)
		p++;
	if (shapito_unlikely(p == end))
		return -1;
	*size -= (uint32_t)(p - *pos) + 1;
	*pos   = p + 1;
	return 0;
}

static inline int
shapito_stream_read(uint32_t n, char **pos, uint32_t *size)
{
	char *end = *pos + *size;
	char *next = *pos + n;
	if (shapito_unlikely(next > end))
		return -1;
	*size -= (uint32_t)(next - *pos);
	*pos   = next;
	return 0;
}

#endif /* SHAPITO_BUF_H */

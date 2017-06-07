#ifndef SO_BUF_H_
#define SO_BUF_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_stream_t so_stream_t;

struct so_stream_t {
	uint8_t *s, *p, *e;
};

static inline int
so_stream_size(so_stream_t *s) {
	return s->e - s->s;
}

static inline int
so_stream_used(so_stream_t *s) {
	return s->p - s->s;
}

static inline int
so_stream_left(so_stream_t *s) {
	return s->e - s->p;
}

static inline void
so_stream_reset(so_stream_t *s) {
	s->p = s->s;
}

static inline void
so_stream_init(so_stream_t *s)
{
	s->s = NULL;
	s->p = NULL;
	s->e = NULL;
}

static inline void
so_stream_free(so_stream_t *s)
{
	if (s->s == NULL)
		return;
	free(s->s);
	s->s = NULL;
	s->p = NULL;
	s->e = NULL;
}

static inline int
so_stream_ensure(so_stream_t *s, int size)
{
	if (s->e - s->p >= size)
		return 0;
	int sz = so_stream_size(s) * 2;
	int actual = so_stream_used(s) + size;
	if (actual > sz)
		sz = actual;
	uint8_t *p = (uint8_t*)realloc(s->s, sz);
	if (p == NULL)
		return -1;
	s->p = p + (s->p - s->s);
	s->e = p + sz;
	s->s = p;
	assert((s->e - s->p) >= size);
	return 0;
}

static inline void
so_stream_advance(so_stream_t *s, int size)
{
	s->p += size;
	assert(s->p <= s->e);
}

static inline void
so_stream_write8(so_stream_t *s, uint8_t v)
{
	*s->p = v;
	so_stream_advance(s, sizeof(uint8_t));
}

static inline void
so_stream_write16(so_stream_t *s, uint16_t v)
{
	s->p[0] = (v >> 8) & 255;
	s->p[1] =  v       & 255;
	so_stream_advance(s, sizeof(uint16_t));
}

static inline void
so_stream_write32(so_stream_t *s, uint32_t v)
{
	s->p[0] = (v >> 24) & 255;
	s->p[1] = (v >> 16) & 255;
	s->p[2] = (v >> 8)  & 255;
	s->p[3] =  v        & 255;
	so_stream_advance(s, sizeof(uint32_t));
}

static inline void
so_stream_write(so_stream_t *s, uint8_t *buf, int size)
{
	memcpy(s->p, buf, size);
	so_stream_advance(s, size);
}

static inline int
so_stream_read8(uint8_t *out, uint8_t **pos, uint32_t *size)
{
	if (so_unlikely(*size < sizeof(uint8_t)))
		return -1;
	*out = *pos[0];
	*size -= sizeof(uint8_t);
	*pos  += sizeof(uint8_t);
	return 0;
}

static inline int
so_stream_read16(uint16_t *out, uint8_t **pos, uint32_t *size)
{
	if (so_unlikely(*size < sizeof(uint16_t)))
		return -1;
	*out = (*pos)[0] << 8 |
	       (*pos)[1];
	*size -= sizeof(uint16_t);
	*pos  += sizeof(uint16_t);
	return 0;
}

static inline int
so_stream_read32(uint32_t *out, uint8_t **pos, uint32_t *size)
{
	if (so_unlikely(*size < sizeof(uint32_t)))
		return -1;
	uint8_t *ptr = *pos;
	*out = ptr[0] << 24 | ptr[1] << 16 |
	       ptr[2] <<  8 | ptr[3];
	*size -= sizeof(uint32_t);
	*pos  += sizeof(uint32_t);
	return 0;
}

static inline int
so_stream_readsz(uint8_t **pos, uint32_t *size)
{
	uint8_t *p = *pos;
	uint8_t *end = p + *size;
	while (p < end && *p)
		p++;
	if (so_unlikely(p == end))
		return -1;
	*size -= (uint32_t)(p - *pos) + 1;
	*pos   = p + 1;
	return 0;
}

static inline int
so_stream_read(uint32_t n, uint8_t **pos, uint32_t *size)
{
	uint8_t *end = *pos + *size;
	uint8_t *next = *pos + n;
	if (so_unlikely(next > end))
		return -1;
	*size -= (uint32_t)(next - *pos);
	*pos   = next;
	return 0;
}

#endif

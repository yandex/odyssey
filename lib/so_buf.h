#ifndef SO_BUF_H_
#define SO_BUF_H_

/*
 * sonata.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct sobuf_t sobuf_t;

struct sobuf_t {
	uint8_t *s, *p, *e;
};

static inline int
so_bufsize(sobuf_t *b) {
	return b->e - b->s;
}

static inline int
so_bufused(sobuf_t *b) {
	return b->p - b->s;
}

static inline int
so_bufleft(sobuf_t *b) {
	return b->e - b->p;
}

static inline void
so_bufreset(sobuf_t *b) {
	b->p = b->s;
}

static inline void
so_bufinit(sobuf_t *b)
{
	b->s = NULL;
	b->p = NULL;
	b->e = NULL;
}

static inline void
so_buffree(sobuf_t *b)
{
	if (b->s == NULL)
		return;
	free(b->s);
	b->s = NULL;
	b->p = NULL;
	b->e = NULL;
}

static inline int
so_bufensure(sobuf_t *b, int size)
{
	if (b->e - b->p >= size)
		return 0;
	int sz = so_bufsize(b) * 2;
	int actual = so_bufused(b) + size;
	if (actual > sz)
		sz = actual;
	uint8_t *p = (uint8_t*)realloc(b->s, sz);
	if (p == NULL)
		return -1;
	b->p = p + (b->p - b->s);
	b->e = p + sz;
	b->s = p;
	assert((b->e - b->p) >= size);
	return 0;
}

static inline void
so_bufadvance(sobuf_t *b, int size)
{
	b->p += size;
	assert(b->p <= b->e);
}

static inline void
so_bufwrite8(sobuf_t *b, uint8_t v)
{
	*b->p = v;
	so_bufadvance(b, sizeof(uint8_t));
}

static inline void
so_bufwrite16(sobuf_t *b, uint16_t v)
{
	b->p[0] = (v >> 8) & 255;
	b->p[1] =  v       & 255;
	so_bufadvance(b, sizeof(uint16_t));
}

static inline void
so_bufwrite32(sobuf_t *b, uint32_t v)
{
	b->p[0] = (v >> 24) & 255;
	b->p[1] = (v >> 16) & 255;
	b->p[2] = (v >> 8)  & 255;
	b->p[3] =  v        & 255;
	so_bufadvance(b, sizeof(uint32_t));
}

static inline void
so_bufwrite(sobuf_t *b, uint8_t *buf, int size)
{
	memcpy(b->p, buf, size);
	so_bufadvance(b, size);
}

static inline int
so_bufread8(uint8_t *out, uint8_t **pos, uint32_t *size)
{
	if (so_unlikely(*size < sizeof(uint8_t)))
		return -1;
	*out = *pos[0];
	*size -= sizeof(uint8_t);
	*pos  += sizeof(uint8_t);
	return 0;
}

static inline int
so_bufread16(uint16_t *out, uint8_t **pos, uint32_t *size)
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
so_bufread32(uint32_t *out, uint8_t **pos, uint32_t *size)
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
so_bufreadsz(uint8_t **pos, uint32_t *size)
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
so_bufread(uint32_t n, uint8_t **pos, uint32_t *size)
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

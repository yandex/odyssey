#ifndef FT_BUF_H_
#define FT_BUF_H_

/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

typedef struct ftbuf ftbuf;

struct ftbuf {
	char *reserve;
	char *s, *p, *e;
};

static inline void
ft_bufinit(ftbuf *b)
{
	b->reserve = NULL;
	b->s = NULL;
	b->p = NULL;
	b->e = NULL;
}

static inline void
ft_bufinit_reserve(ftbuf *b, void *buf, int size)
{
	b->reserve = buf;
	b->s = buf;
	b->p = b->s; 
	b->e = b->s + size;
}

static inline void
ft_buffree(ftbuf *b)
{
	if (b->s == NULL)
		return;
	if (b->s != b->reserve)
		free(b->s);
	b->s = NULL;
	b->p = NULL;
	b->e = NULL;
}

static inline int
ft_bufsize(ftbuf *b) {
	return b->e - b->s;
}

static inline int
ft_bufused(ftbuf *b) {
	return b->p - b->s;
}

static inline int
ft_bufunused(ftbuf *b) {
	return b->e - b->p;
}

static inline void
ft_bufreset(ftbuf *b) {
	b->p = b->s;
}

static inline int
ft_bufensure(ftbuf *b, int size)
{
	if (b->e - b->p >= size)
		return 0;
	int sz = ft_bufsize(b) * 2;
	int actual = ft_bufused(b) + size;
	if (actual > sz)
		sz = actual;
	char *p;
	if (b->s == b->reserve) {
		p = malloc(sz);
		if (p == NULL)
			return -1;
		memcpy(p, b->s, ft_bufused(b));
	} else {
		p = realloc(b->s, sz);
		if (p == NULL)
			return -1;
	}
	b->p = p + (b->p - b->s);
	b->e = p + sz;
	b->s = p;
	assert((b->e - b->p) >= size);
	return 0;
}

static inline void
ft_bufadvance(ftbuf *b, int size)
{
	b->p += size;
}

static inline int
ft_bufadd(ftbuf *b, void *buf, int size)
{
	int rc = ft_bufensure(b, size);
	if (rc == -1)
		return -1;
	memcpy(b->p, buf, size);
	ft_bufadvance(b, size);
	return 0;
}

#endif

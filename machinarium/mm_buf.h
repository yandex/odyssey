#ifndef MM_BUF_H_
#define MM_BUF_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

typedef struct mmbuf mmbuf;

struct mmbuf {
	char *reserve;
	char *s, *p, *e;
};

static inline void
mm_bufinit(mmbuf *b)
{
	b->reserve = NULL;
	b->s = NULL;
	b->p = NULL;
	b->e = NULL;
}

static inline void
mm_bufinit_reserve(mmbuf *b, void *buf, int size)
{
	b->reserve = buf;
	b->s = buf;
	b->p = b->s; 
	b->e = b->s + size;
}

static inline void
mm_buffree(mmbuf *b)
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
mm_bufsize(mmbuf *b) {
	return b->e - b->s;
}

static inline int
mm_bufused(mmbuf *b) {
	return b->p - b->s;
}

static inline int
mm_bufunused(mmbuf *b) {
	return b->e - b->p;
}

static inline void
mm_bufreset(mmbuf *b) {
	b->p = b->s;
}

static inline int
mm_bufensure(mmbuf *b, int size)
{
	if (b->e - b->p >= size)
		return 0;
	int sz = mm_bufsize(b) * 2;
	int actual = mm_bufused(b) + size;
	if (actual > sz)
		sz = actual;
	char *p;
	if (b->s == b->reserve) {
		p = malloc(sz);
		if (p == NULL)
			return -1;
		memcpy(p, b->s, mm_bufused(b));
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
mm_bufadvance(mmbuf *b, int size)
{
	b->p += size;
}

static inline int
mm_bufadd(mmbuf *b, void *buf, int size)
{
	int rc = mm_bufensure(b, size);
	if (rc == -1)
		return -1;
	memcpy(b->p, buf, size);
	mm_bufadvance(b, size);
	return 0;
}

#endif

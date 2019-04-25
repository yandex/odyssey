#ifndef MEMUTILS_H
#define MEMUTILS_H

#ifdef MCXT_CHECK
#include "memcheck.h"
#else
#define VALGRIND_MAKE_MEM_DEFINED(ptr,size)
#define VALGRIND_CHECK_VALUE_IS_DEFINED(ptr)
#endif

#define MAXIMUM_ALIGNOF 8
#define TYPEALIGN(ALIGNVAL,LEN)  \
	(((uintptr_t) (LEN) + ((ALIGNVAL) - 1)) & ~((uintptr_t) ((ALIGNVAL) - 1)))

#define MAXALIGN(LEN)	TYPEALIGN(MAXIMUM_ALIGNOF, (LEN))

typedef enum __attribute__ ((__packed__)) {
	mct_alloc		= 0x01,
	mct_context		= 0x02
} mcxt_chunk_type;

static_assert(sizeof(mcxt_chunk_type) == 1, "enum size should be 1");

struct mcxt_memory_chunk
{
	mcxt_chunk_t		prev;
	mcxt_chunk_t		next;
#ifdef MCXT_CHECK
	uint16_t			refcount;
	mcxt_chunk_type		chunk_type;
#define ALLOCCHUNK_RAWSIZE  (1 + SIZEOF_VOID_P * 2 + 2)
#else
#define ALLOCCHUNK_RAWSIZE  (SIZEOF_VOID_P * 2)
#endif

	/* ensure proper alignment by adding padding if needed */
#if (ALLOCCHUNK_RAWSIZE % MAXIMUM_ALIGNOF) != 0
	char	padding[MAXIMUM_ALIGNOF - ALLOCCHUNK_RAWSIZE % MAXIMUM_ALIGNOF];
#endif
	mcxt_context_t		context;
};

#define MEMORY_CHUNK_SIZE (MAXALIGN(sizeof(struct mcxt_memory_chunk)))
#define GetMemoryChunk(p) ((mcxt_chunk_t)((char *)(p) - MEMORY_CHUNK_SIZE))
#define ChunkDataOffset(c) ((void *)((char *)(c) + MEMORY_CHUNK_SIZE))

#endif

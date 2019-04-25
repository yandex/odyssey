#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "sleep_lock.h"
#include "memconsts.h"
#include "memcontext.h"
#include "memutils.h"

__thread mcxt_context_t current_mcxt = NULL;

/* Append chunk at the end of list of chunks */
static inline void
mcxt_append_chunk(mcxt_context_t context, mcxt_chunk_t chunk)
{
	mcxt_chunk_t		lastchunk;

	lastchunk = context->lastchunk;
	if (lastchunk)
		lastchunk->next = chunk;

	context->lastchunk = chunk;
	chunk->prev = lastchunk;
	chunk->next = NULL;
}

/* Link child context to parent context */
static void mcxt_link_context(mcxt_context_t parent, mcxt_context_t new)
{
	mm_sleeplock_lock(&parent->lock);
	if (parent->firstchild == NULL)
		parent->firstchild = new;
	else
	{
		mcxt_context_t	child;

		/* find latest child */
		for (child = parent->firstchild; child->nextchild != NULL;
				child = child->nextchild);

		child->nextchild = new;
		new->prevchild = child;
	}
	mm_sleeplock_unlock(&parent->lock);
}

/* New memory context */
mcxt_context_t mcxt_new(mcxt_context_t parent)
{
	mcxt_chunk_t chunk = calloc(MEMORY_CHUNK_SIZE +
		sizeof(struct mcxt_context_data), 1);
	mcxt_context_t new = ChunkDataOffset(chunk);
	memset(chunk, 0, sizeof(struct mcxt_context_data) + MEMORY_CHUNK_SIZE);

	if (new == NULL)
		return NULL;

	new->parent = parent;

#ifdef MCXT_CHECK
	new->ptid = pthread_self();
	chunk->chunk_type = mct_context;
#endif

	/* append to parent's children */
	if (parent)
		mcxt_link_context(parent, new);

	return new;
}

/* Change current memory context */
mcxt_context_t mcxt_switch_to(mcxt_context_t to)
{
	mcxt_context_t	old = current_mcxt;
	current_mcxt = to;
	return old;
}

/* Free all memory in memory context */
void mcxt_reset(mcxt_context_t context, bool recursive)
{
	mcxt_context_callback_t	*cb = context->reset_cbs;
	mcxt_chunk_t		chunk = context->lastchunk;

	if (recursive)
	{
		/* reset children if recursive */
		mcxt_context_t	child;
		for (child = context->firstchild; child != NULL; child = child->nextchild)
		{
			mcxt_reset(child, true);
		}
	}

	/* call reset callbacks */
	while (cb != NULL)
	{
		cb->func(cb->arg);
		cb = cb->next;
	}

	while (chunk != NULL)
	{
		mcxt_chunk_t prev = chunk->prev;
		free(chunk);
		chunk = prev;
	}
	context->lastchunk = NULL;
}

int mcxt_chunks_count(mcxt_context_t context)
{
	int			count = 0;
	mcxt_chunk_t chunk = context->lastchunk;
	while (chunk != NULL)
	{
		count++;
		chunk = chunk->prev;
	}
	return count;
}

static void mcxt_unlink_context(mcxt_context_t context)
{
	if (context->parent == NULL)
	{
		assert(context->prevchild == NULL);
		assert(context->nextchild == NULL);
		return;
	}

	mm_sleeplock_lock(&context->parent->lock);

	if (context->prevchild)
		context->prevchild->nextchild = context->nextchild;
	else
		context->parent->firstchild = context->nextchild;

	if (context->nextchild)
		context->nextchild->prevchild = context->prevchild;

	mm_sleeplock_unlock(&context->parent->lock);
}

/* Delete memory context, all its chunks and childs */
void mcxt_delete(mcxt_context_t context)
{
	mcxt_context_t	child;

	assert(current_mcxt != context);
#ifdef MCXT_CHECK
	assert(GetMemoryChunk(context)->chunk_type == mct_context);
#endif

	mcxt_unlink_context(context);

	for (child = context->firstchild; child != NULL; child = child->nextchild)
	{
		mcxt_delete(child);
	}

	mcxt_reset(context, false);
	mcxt_clean_reset_callbacks(context);
	free(GetMemoryChunk(context));
}

/*
 * Allocate memory in specified memory context,
 * we don't have locks here since we assume that threads should not use same
 * memory contexts for allocations.
 */
void *mcxt_alloc_mem(mcxt_context_t context, size_t size, bool zero)
{
	mcxt_chunk_t		chunk;

	assert(context);
	chunk = malloc(MEMORY_CHUNK_SIZE + size);
	if (chunk == NULL)
		return NULL;

	if (zero)
		memset(chunk, 0, MEMORY_CHUNK_SIZE + size);

#ifdef MCXT_CHECK
	assert(context->ptid == pthread_self());
	chunk->chunk_type = mct_alloc;
	chunk->refcount = 0;
#endif

	chunk->prev = chunk->next = NULL;
	chunk->context = context;
	mcxt_append_chunk(context, chunk);

	VALGRIND_MAKE_MEM_DEFINED(chunk, MEMORY_CHUNK_SIZE);
	return ChunkDataOffset(chunk);
}

void *mcxt_realloc(void *ptr, size_t size)
{
	mcxt_chunk_t chunk = GetMemoryChunk(ptr);
	assert(size > MEMORY_CHUNK_SIZE);
	return realloc(chunk, size);
}

/* Free memory in specified memory context */
void mcxt_free_mem(mcxt_context_t context, void *p)
{
	VALGRIND_CHECK_VALUE_IS_DEFINED(context);
	VALGRIND_CHECK_VALUE_IS_DEFINED(p);

	mcxt_chunk_t chunk = GetMemoryChunk(p);

	assert(p != NULL);
	assert(p == (void *) MAXALIGN(p));
#ifdef MCXT_CHECK
	assert(chunk->chunk_type == mct_alloc);
	assert(chunk->refcount == 0);
#endif

	/* first, deattach from chunks in context */
	if (chunk->next)
		chunk->next->prev = chunk->prev;
	if (chunk->prev)
		chunk->prev->next = chunk->next;
	if (chunk == context->lastchunk)
		context->lastchunk = chunk->prev;

	free(chunk);
}

char *mcxt_strdup_in(mcxt_context_t context, const char *string)
{
	char	   *nstr;
	size_t		len = strlen(string) + 1;

	nstr = (char *) mcxt_alloc_mem(context, len, false);
	memcpy(nstr, string, len);
	return nstr;
}

/* Add callback for mcxt_reset call on the memory context */
void mcxt_add_reset_callback(mcxt_context_t context,
		mcxt_callback_function func, void *arg)
{
	mcxt_context_callback_t	*cb,
							*new;

	new = malloc(sizeof(mcxt_context_callback_t));
	new->func = func;
	new->arg = arg;
	new->next = NULL;
	mm_sleeplock_lock(&context->lock);

	if (context->reset_cbs == NULL)
		context->reset_cbs = new;
	else
	{
		/* find end of chain */
		for (cb = context->reset_cbs; cb->next != NULL;)
			cb = cb->next;

		cb->next = new;
	}
	mm_sleeplock_unlock(&context->lock);
}

/* Reset all `mcxt_reset` callbacks on the memory context */
void mcxt_clean_reset_callbacks(mcxt_context_t context)
{
	mcxt_context_callback_t *cb;

	mm_sleeplock_lock(&context->lock);
	cb = context->reset_cbs;
	context->reset_cbs = NULL;

	while (cb != NULL)
	{
		mcxt_context_callback_t *next = cb->next;
		free(cb);
		cb = next;
	}
	mm_sleeplock_unlock(&context->lock);
}

#ifdef MCXT_CHECK
void mcxt_incr_refcount(void *ptr)
{
	GetMemoryChunk(ptr)->refcount++;
}

void mcxt_decr_refcount(void *ptr)
{
	GetMemoryChunk(ptr)->refcount--;
}
void mcxt_check(void *ptr, void *context, int refcount)
{
	assert(GetMemoryChunk(ptr)->context == context);
	assert(GetMemoryChunk(ptr)->refcount == refcount);
}
#endif

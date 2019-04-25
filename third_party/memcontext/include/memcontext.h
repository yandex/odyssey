#ifndef MEMCONTEXT_H
#define MEMCONTEXT_H

#include <stdbool.h>

typedef struct mcxt_memory_chunk		*mcxt_chunk_t;
typedef struct mcxt_context_data		*mcxt_context_t;
typedef struct mcxt_context_callback	 mcxt_context_callback_t;

typedef void (*mcxt_callback_function) (void *arg);
struct mcxt_context_callback
{
	mcxt_callback_function func;		/* function to call */
	void	   *arg;					/* argument to pass it */
	mcxt_context_callback_t *next;		/* next in list of callbacks */
};

struct mcxt_context_data
{
	uint32_t		lock;
	mcxt_context_t	parent;
	mcxt_context_t	firstchild;			/* link to first child */
	mcxt_context_t	prevchild;			/* prev parent's child */
	mcxt_context_t	nextchild;			/* next parent's child */
	mcxt_chunk_t	lastchunk;			/* list of allocated chunks */
	mcxt_context_callback_t *reset_cbs;	/* list of reset/delete callbacks */
#ifdef MCXT_CHECK
	pthread_t		ptid;				/* id of owner thread */
#endif
};

extern __thread mcxt_context_t current_mcxt;

mcxt_context_t mcxt_new(mcxt_context_t parent);
mcxt_context_t mcxt_switch_to(mcxt_context_t to);

void mcxt_reset(mcxt_context_t context, bool recursive);
void mcxt_delete(mcxt_context_t context);
void *mcxt_alloc_mem(mcxt_context_t context, size_t size, bool zero);
void *mcxt_realloc(void *ptr, size_t size);
void mcxt_free_mem(mcxt_context_t context, void *p);
void mcxt_clean_reset_callbacks(mcxt_context_t context);
void mcxt_add_reset_callback(mcxt_context_t context, mcxt_callback_function func, void *arg);

int mcxt_chunks_count(mcxt_context_t context);
char *mcxt_strdup_in(mcxt_context_t context, const char *string);

static inline void *mcxt_alloc(size_t size)
{
	assert(current_mcxt != NULL);
	return mcxt_alloc_mem(current_mcxt, size, false);
}

static inline void *mcxt_alloc0(size_t size)
{
	assert(current_mcxt != NULL);
	return mcxt_alloc_mem(current_mcxt, size, true);
}

static inline void mcxt_free(void *p)
{
	mcxt_context_t context = *((mcxt_context_t *) ((char *) p - sizeof(void *)));
	mcxt_free_mem(context, p);
}

static inline char *mcxt_strdup(const char *string)
{
	assert(current_mcxt != NULL);
	return mcxt_strdup_in(current_mcxt, string);
}

#ifdef MCXT_CHECK
void mcxt_incr_refcount(void *ptr);
void mcxt_decr_refcount(void *ptr);
void mcxt_check(void *ptr, void *context, int refcount);
#else
#define mcxt_incr_refcount(ptr)
#define mcxt_decr_refcount(ptr)
#endif

#endif


/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/bio.h>

void mm_bio_init(mm_bio_t *bio, const mm_bio_methods_t *methods, mm_io_t *io,
		 void *state)
{
	bio->methods = methods;
	bio->next = NULL;
	bio->io = io;
	bio->state = state;
}

void mm_bio_push(mm_bio_t **top, mm_bio_t *layer)
{
	layer->next = *top;
	*top = layer;
}

void mm_bio_free(mm_bio_t *bio)
{
	bio->methods->free(bio);
}

void mm_bio_free_chain(mm_bio_t *bio)
{
	while (bio) {
		mm_bio_t *next = bio->next;
		mm_bio_free(bio);
		bio = next;
	}
}


/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>

int
main(int argc, char *argv[])
{
	mm_t env = mm_new();
	mmio_t io = mm_io_new(env);
	mm_close(io);
	mm_free(env);
	return 0;
}


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
	mm_free(env);
	return 0;
}

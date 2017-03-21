
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>

int
main(int argc, char *argv[])
{
	machine_t machine = machine_create();
	machine_io_t io = machine_create_io(machine);
	machine_close(io);
	machine_free(machine);
	return 0;
}


/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

typedef struct mm_t mm_t;

struct mm_t {
	pthread_spinlock_t lock;
	mm_list_t          machine_list;
	int                machine_count;
	int                machine_seq;
};

static mm_t machinarium;

void
mm_attach(mm_machine_t *machine)
{
	pthread_spin_lock(&machinarium.lock);
	machine->id = machinarium.machine_seq++;
	mm_list_append(&machinarium.machine_list, &machine->link);
	machinarium.machine_count++;
	pthread_spin_unlock(&machinarium.lock);
}

void
mm_detach(mm_machine_t *machine)
{
	pthread_spin_lock(&machinarium.lock);
	mm_list_unlink(&machine->link);
	machinarium.machine_count--;
	pthread_spin_unlock(&machinarium.lock);
}

int
mm_detach_by_id(int id, mm_machine_t **ret)
{
	pthread_spin_lock(&machinarium.lock);
	mm_list_t *i;
	mm_list_foreach(&machinarium.machine_list, i) {
		mm_machine_t *machine;
		machine = mm_container_of(i, mm_machine_t, link);
		if (machine->id == id) {
			mm_list_unlink(&machine->link);
			machinarium.machine_count--;
			*ret = machine;
			pthread_spin_unlock(&machinarium.lock);
			return 0;
		}
	}
	pthread_spin_unlock(&machinarium.lock);
	return -1;
}

MACHINE_API int
machinarium_init(void)
{
	pthread_spin_init(&machinarium.lock, PTHREAD_PROCESS_PRIVATE);
	mm_list_init(&machinarium.machine_list);
	machinarium.machine_seq = 0;
	machinarium.machine_count = 0;
	mm_tls_init();
	return 0;
}

MACHINE_API void
machinarium_free(void)
{
	pthread_spin_destroy(&machinarium.lock);
}

/*
 * machinarium.
 *
 * Scoped transfer of a coroutine to another machine and back.
 */

#include <errno.h>
#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/mm.h>

/* Keep a C invocation boundary even with LTO: code using thread-local storage
 * must not be inlined/hoisted into the function that crosses threads. */
__attribute__((noinline)) static void
mm_machine_invoke(machine_coroutine_t function, void *arg)
{
	function(arg);
}

int mm_machine_running(mm_machine_t *machine)
{
	if (atomic_load(&machine->online) &&
	    mm_scheduler_online(&machine->scheduler)) {
		return 1;
	}
	mm_spinlock_lock(&machinarium.machine_mgr.lock);
	int running = atomic_load(&machine->visits) ||
		      (atomic_load(&machine->online) &&
		       mm_scheduler_online(&machine->scheduler));
	if (!running) {
		/* No new transfer can acquire this machine after this point. */
		machine->accepting = 0;
	}
	mm_spinlock_unlock(&machinarium.machine_mgr.lock);
	return running;
}

void mm_machine_dispatch(mm_coroutine_t *coroutine)
{
	mm_machine_t *destination = coroutine->destination;
	mm_spinlock_lock(&machinarium.machine_mgr.lock);
	coroutine->location = destination;
	int wakeup = mm_list_empty(&destination->incoming);
	mm_list_append(&destination->incoming, &coroutine->link);
	atomic_store(&destination->incoming_pending, 1);
	if (wakeup) {
		mm_eventmgr_wakeup(
			mm_eventmgr_wakeup_fd(&destination->event_mgr));
	}
	mm_spinlock_unlock(&machinarium.machine_mgr.lock);
}

void mm_machine_receive(void)
{
	mm_machine_t *machine = mm_self;
	if (atomic_exchange(&machine->incoming_pending, 0)) {
		mm_spinlock_lock(&machinarium.machine_mgr.lock);
		while (!mm_list_empty(&machine->incoming)) {
			mm_coroutine_t *coroutine = mm_container_of(
				machine->incoming.next, mm_coroutine_t, link);
			mm_scheduler_set(&machine->scheduler, coroutine,
					 MM_CREADY);
		}
		mm_spinlock_unlock(&machinarium.machine_mgr.lock);
	}

	if (atomic_exchange(&machine->cancel_pending, 0)) {
		mm_list_t *i, *next;
		mm_list_foreach_safe (&machine->scheduler.list_active, i,
				      next) {
			mm_coroutine_t *coroutine =
				mm_container_of(i, mm_coroutine_t, link);
			if (atomic_load(&coroutine->cancel_requested)) {
				mm_coroutine_cancel(coroutine);
			}
		}
		/* Ready coroutines observe cancellation before being resumed. */
	}
}

MACHINE_API int machine_coroutine_call_on(uint64_t machine_id,
					  machine_coroutine_t function,
					  void *arg)
{
	mm_machine_t *home = mm_self;
	mm_coroutine_t *coroutine = home->scheduler.current;
	if (coroutine->visiting || coroutine->call_ptr || !function) {
		coroutine->errno_ = EINVAL;
		return -1;
	}
	if (mm_coroutine_is_cancelled(coroutine)) {
		coroutine->errno_ = ECANCELED;
		return -1;
	}
	if (home->id == machine_id) {
		coroutine->visiting = 1;
		mm_machine_invoke(function, arg);
		coroutine->visiting = 0;
		return 0;
	}

	mm_machinemgr_t *mgr = &machinarium.machine_mgr;
	mm_machine_t *destination = NULL;
	mm_spinlock_lock(&mgr->lock);
	mm_list_t *i;
	mm_list_foreach (&mgr->list, i) {
		mm_machine_t *candidate =
			mm_container_of(i, mm_machine_t, link);
		if (candidate->id == machine_id && candidate->accepting &&
		    atomic_load(&candidate->online)) {
			destination = candidate;
			atomic_fetch_add(&destination->visits, 1);
			atomic_fetch_add(&home->visits, 1);
			break;
		}
	}
	mm_spinlock_unlock(&mgr->lock);
	if (!destination) {
		coroutine->errno_ = ENOENT;
		return -1;
	}

	coroutine->visiting = 1;
	coroutine->destination = destination;
	mm_scheduler_set(&home->scheduler, coroutine, MM_CMIGRATING);
	mm_scheduler_yield(&home->scheduler);

	/* On the destination. Use captured pointers here, never a cached mm_self
	 * or thread-local address across a transfer. The callback starts a fresh
	 * C invocation on this thread and must finish all its local waits/I/O. */
	int rc = 0;
	if (mm_coroutine_is_cancelled(coroutine)) {
		coroutine->errno_ = ECANCELED;
		rc = -1;
	} else {
		mm_machine_invoke(function, arg);
	}

	coroutine->destination = home;
	mm_scheduler_set(&destination->scheduler, coroutine, MM_CMIGRATING);
	mm_scheduler_yield(&destination->scheduler);

	/* Back home. Both threads stay alive until the return is complete, even
	 * if their main coroutines have exited or stop was requested. */
	coroutine->visiting = 0;
	coroutine->destination = NULL;
	mm_spinlock_lock(&mgr->lock);
	atomic_fetch_sub(&home->visits, 1);
	if (atomic_fetch_sub(&destination->visits, 1) == 1) {
		/* Only the last visitor can make a draining machine exit. */
		mm_eventmgr_wakeup(
			mm_eventmgr_wakeup_fd(&destination->event_mgr));
	}
	mm_spinlock_unlock(&mgr->lock);
	return rc;
}

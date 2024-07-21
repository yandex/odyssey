import gdb

from contextlib import contextmanager

gdb.write('Loading machinarium runtime support for gdb...', stream=gdb.STDLOG)


MM_LIST_NEXT_FIELD_NAME = "next"

MM_COROUTINE_TYPE_NAME = "struct mm_coroutine"
MM_COROUTINE_LINK_FIELD_NAME = "link"
MM_COROUTIME_ID_FIELD_NAME = "id"
MM_COROUTINE_FUNCTION_FIELD_NAME = "function"
MM_COROUTINE_STATE_FIELD_NAME = "state"
MM_COROUTINE_ERRNO_FIELD_NAME = "errno_"

MM_SELF_VARIABLE_NAME = "mm_self"

MM_MACHINE_SCHEDULER_FIELD_NAME = "scheduler"

MM_SCHEDULER_CURRENT_FIELD_NAME = "current"
MM_SCHEDULER_COUNT_READY_FIELD_NAME = "count_ready"
MM_SCHEDULER_COUNT_ACTIVE_FIELD_NAME = "count_active"
MM_SCHEDULER_LIST_READY_FIELD_NAME = "list_ready"
MM_SCHEDULER_LIST_ACTIVE_FIELD_NAME = "list_active"


GDB_CHAR_POINTER_TYPE = gdb.lookup_type("char").pointer()
GDB_MM_COROUTINE_TYPE = gdb.lookup_type(MM_COROUTINE_TYPE_NAME)
GDB_MM_COROUTINE_POINTER_TYPE = GDB_MM_COROUTINE_TYPE.pointer()


def get_mm_self_or_none():
    try:
        return gdb.parse_and_eval(MM_SELF_VARIABLE_NAME)
    except gdb.error:
        return None


def get_mm_coroutine_link_offset():
    for f in GDB_MM_COROUTINE_TYPE.fields():
        if f.name == MM_COROUTINE_LINK_FIELD_NAME:
            return f.bitpos // 8

    raise KeyError(
        f'{MM_COROUTINE_LINK_FIELD_NAME} field not found in type {GDB_MM_COROUTINE_TYPE}')


MM_COROUTINE_LINK_FIELD_OFFSET = get_mm_coroutine_link_offset()


@contextmanager
def gdb_thread_restore():
    try:
        current_thread = gdb.selected_thread()
        yield current_thread
    finally:
        if current_thread is not None:
            current_thread.switch()


@contextmanager
def gdb_frame_restore():
    try:
        current_frame = gdb.selected_frame()
        yield current_frame
    finally:
        if current_frame is not None:
            current_frame.select()


class MMCoroutines(gdb.Command):
    """List all coroutines. Usage:
    info mmcoros         - list coroutines for all threads
    info mmcoros <id(s)> - list coroutines for specified thread(s). <id> can be thread id or name.
    """

    def __init__(self):
        super(MMCoroutines, self).__init__(
            "info mmcoros", gdb.COMMAND_STACK, gdb.COMPLETE_NONE)

    def _get_threads_list_from_args(self, args):
        ids = set(gdb.string_to_argv(args))

        if len(ids) == 0:
            return gdb.selected_inferior().threads()

        threads_list = []
        for thr in gdb.selected_inferior().threads():
            if thr.name in ids or str(thr.num) in ids:
                threads_list.append(thr)

        return threads_list

    def _print_coroutines_list(self, coroutines_list, count, current_coroutine_id):
        if count == 0:
            return

        next_list_node_ptr = coroutines_list

        for _ in range(count):
            next_list_node_ptr = next_list_node_ptr[MM_LIST_NEXT_FIELD_NAME]
            nlnp_as_char_ptr = next_list_node_ptr.cast(GDB_CHAR_POINTER_TYPE)
            next_coroutine_char_ptr = nlnp_as_char_ptr - MM_COROUTINE_LINK_FIELD_OFFSET
            next_coroutine_ptr = next_coroutine_char_ptr.cast(
                GDB_MM_COROUTINE_POINTER_TYPE
            )
            next_coroutine = next_coroutine_ptr.dereference()
            coro_id = next_coroutine[MM_COROUTIME_ID_FIELD_NAME]
            coro_state = next_coroutine[MM_COROUTINE_STATE_FIELD_NAME]
            coro_errno = next_coroutine[MM_COROUTINE_ERRNO_FIELD_NAME]
            coro_func = next_coroutine[MM_COROUTINE_FUNCTION_FIELD_NAME]
            current_coro_pref = ' ' if coro_id != current_coroutine_id else '*'

            gdb.write(
                f'{current_coro_pref}{coro_id}\t{coro_state}\t{coro_errno}\t{coro_func}\n')

    def _list_coroutines_for_thread(self, thread):
        thread.switch()

        gdb.write(
            f"Thread {thread.num} ({thread.name}) machinarium coroutines:\n")

        mm_self_ptr = get_mm_self_or_none()
        if mm_self_ptr is None:
            gdb.write(
                f" There is no {MM_SELF_VARIABLE_NAME} in the current context. Does the executable actually use the machinarium framework?\n")
            return

        if mm_self_ptr == 0:
            gdb.write(
                f" The {MM_SELF_VARIABLE_NAME} is NULL, so no coroutines in this thread available.\n")
            return

        mm_self_val = mm_self_ptr.dereference()
        scheduler = mm_self_val[MM_MACHINE_SCHEDULER_FIELD_NAME]
        current_coroutine_ptr = scheduler[MM_SCHEDULER_CURRENT_FIELD_NAME]
        current_coroutine = current_coroutine_ptr.dereference()
        current_coroutine_id = current_coroutine[MM_COROUTIME_ID_FIELD_NAME]
        count_ready = scheduler[MM_SCHEDULER_COUNT_READY_FIELD_NAME]
        count_active = scheduler[MM_SCHEDULER_COUNT_ACTIVE_FIELD_NAME]
        list_active = scheduler[MM_SCHEDULER_LIST_ACTIVE_FIELD_NAME]
        list_ready = scheduler[MM_SCHEDULER_LIST_READY_FIELD_NAME]

        gdb.write(" Id\tState\t\terrno\tFunction\n")

        self._print_coroutines_list(
            list_active, count_active, current_coroutine_id)
        self._print_coroutines_list(
            list_ready, count_ready, current_coroutine_id)

    def invoke(self, args, is_tty):
        with gdb_thread_restore():
            for thread in self._get_threads_list_from_args(args):
                self._list_coroutines_for_thread(thread)
                gdb.write("\n")


MMCoroutines()


gdb.write('done.\n', stream=gdb.STDLOG)

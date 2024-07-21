import gdb
import uuid

from contextlib import contextmanager

gdb.write('Loading machinarium runtime support for gdb...', stream=gdb.STDLOG)


# see list.h
MM_LIST_NEXT_FIELD_NAME = "next"

# see context.h
MM_CONTEXT_SP_FIELD_NAME = "sp"

# see coroutine.h
MM_COROUTINE_TYPE_NAME = "struct mm_coroutine"
MM_COROUTINE_LINK_FIELD_NAME = "link"
MM_COROUTINE_ID_FIELD_NAME = "id"
MM_COROUTINE_FUNCTION_FIELD_NAME = "function"
MM_COROUTINE_STATE_FIELD_NAME = "state"
MM_COROUTINE_ERRNO_FIELD_NAME = "errno_"
MM_COROUTINE_CONTEXT_FIELD_NAME = "context"

# see machine.c
MM_SELF_VARIABLE_NAME = "mm_self"

# see machine.h
MM_MACHINE_SCHEDULER_FIELD_NAME = "scheduler"

# see scheduler.h
MM_SCHEDULER_CURRENT_FIELD_NAME = "current"
MM_SCHEDULER_COUNT_READY_FIELD_NAME = "count_ready"
MM_SCHEDULER_COUNT_ACTIVE_FIELD_NAME = "count_active"
MM_SCHEDULER_LIST_READY_FIELD_NAME = "list_ready"
MM_SCHEDULER_LIST_ACTIVE_FIELD_NAME = "list_active"


GDB_CHAR_POINTER_TYPE = gdb.lookup_type("char").pointer()
GDB_MM_COROUTINE_TYPE = gdb.lookup_type(MM_COROUTINE_TYPE_NAME)
GDB_MM_COROUTINE_POINTER_TYPE = GDB_MM_COROUTINE_TYPE.pointer()


def parse_int_or_none(s):
    try:
        return int(s)
    except:
        return None


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


def gdb_get_current_platform():
    arch = gdb.selected_inferior().architecture()

    return arch.name()


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


@contextmanager
def gdb_pc_sp_set_and_restore(newpc, newsp):
    # note that assigment to sp must be performed in the top frame
    # so this function executes select-frame 0
    # and should be used only under gdb_frame_restore

    suffix = str(uuid.uuid4()).replace('-', '_')

    save_sp_varname = f"save_sp_{suffix}"
    save_pc_varname = f"save_pc_{suffix}"

    try:
        gdb.parse_and_eval(f"${save_sp_varname} = $sp")
        gdb.parse_and_eval(f"${save_pc_varname} = $pc")

        gdb.execute("select-frame 0")
        gdb.parse_and_eval(f"$sp = {newsp}")
        gdb.parse_and_eval(f"$pc = {newpc}")

        yield
    finally:
        gdb.execute("select-frame 0")
        gdb.parse_and_eval(f"$sp = ${save_sp_varname}")
        gdb.parse_and_eval(f"$pc = ${save_pc_varname}")


def mm_iterate_coroutines_list(coroutines_list, count):
    coros = []

    if count == 0:
        return coros

    next_list_node_ptr = coroutines_list

    for _ in range(count):
        next_list_node_ptr = next_list_node_ptr[MM_LIST_NEXT_FIELD_NAME]
        # The actual coroutine pointer is behind of link field offset from mm_coroutine type
        # So we must perform actions like in mm_container_of
        nlnp_as_char_ptr = next_list_node_ptr.cast(GDB_CHAR_POINTER_TYPE)
        next_coroutine_char_ptr = nlnp_as_char_ptr - MM_COROUTINE_LINK_FIELD_OFFSET
        next_coroutine_ptr = next_coroutine_char_ptr.cast(
            GDB_MM_COROUTINE_POINTER_TYPE
        )
        next_coroutine = next_coroutine_ptr.dereference()
        coros.append(next_coroutine)

    return coros


def mm_get_current_thread_coroutine_id():
    mm_self_ptr = get_mm_self_or_none()
    if mm_self_ptr is None or mm_self_ptr == 0:
        return None

    mm_self_val = mm_self_ptr.dereference()
    scheduler = mm_self_val[MM_MACHINE_SCHEDULER_FIELD_NAME]
    current_coroutine_ptr = scheduler[MM_SCHEDULER_CURRENT_FIELD_NAME]
    current_coroutine_id = None
    if current_coroutine_ptr != 0:
        current_coroutine = current_coroutine_ptr.dereference()
        current_coroutine_id = current_coroutine[MM_COROUTINE_ID_FIELD_NAME]

    return current_coroutine_id


def mm_current_thread_coroutines():
    mm_self_ptr = get_mm_self_or_none()
    if mm_self_ptr is None or mm_self_ptr == 0:
        return []

    mm_self_val = mm_self_ptr.dereference()
    scheduler = mm_self_val[MM_MACHINE_SCHEDULER_FIELD_NAME]
    count_ready = scheduler[MM_SCHEDULER_COUNT_READY_FIELD_NAME]
    count_active = scheduler[MM_SCHEDULER_COUNT_ACTIVE_FIELD_NAME]
    list_active = scheduler[MM_SCHEDULER_LIST_ACTIVE_FIELD_NAME]
    list_ready = scheduler[MM_SCHEDULER_LIST_READY_FIELD_NAME]

    active_coroutines = mm_iterate_coroutines_list(list_active, count_active)
    ready_coroutines = mm_iterate_coroutines_list(list_ready, count_ready)

    return active_coroutines + ready_coroutines


def mm_find_thread(thread_id):
    thread = None
    for thr in gdb.selected_inferior().threads():
        if thr.name == thread_id or str(thr.num) == thread_id:
            thread = thr
            break

    return thread


def mm_find_coroutine_in_current_thread(target_coro_id):
    for coro in mm_current_thread_coroutines():
        coro_id = coro[MM_COROUTINE_ID_FIELD_NAME]

        if target_coro_id == int(coro_id):
            return coro

    return None


def mm_get_pc_sp_for_coroutine_x64(coroutine):
    context = coroutine[MM_COROUTINE_CONTEXT_FIELD_NAME]
    raw_sp = int(context[MM_CONTEXT_SP_FIELD_NAME])

    # Some magic needs to be performed, lets see
    # There is the stack (raw_sp) 'inside' mm_context_switch:
    #
    # low addr |   r15                 <- raw_sp
    #          |   r14
    #          |   r13
    #          |   r12
    #          |   rbx
    #          |   rbp
    #          |   [return address]    <- desired pc
    # high addr|   [frame of the coro] <- desired sp
    #
    # So, to 'restore' sp we need skip saved registers and return address
    # And to 'restore' pc we need to set it by return address

    saved_registers_size = 6 * 8
    pc_ptr = raw_sp + saved_registers_size

    pc = gdb.parse_and_eval(f'(uint64_t*)({pc_ptr})').dereference()
    sp = raw_sp + saved_registers_size + 8  # last 8 is return address

    return pc, sp


class MMCoroutines(gdb.Command):
    """List all coroutines. Usage:
    info mmcoros         - list coroutines for all threads
    info mmcoros <id(s)> - list coroutines for specified thread(s). <id> can be thread id or name.

Examples:
    (gdb) info mmcoros
    (gdb) info mmcoros thread-name1 42
    (gdb) info mmcoros 41 42 43
    (gdb) info mmcoros thread-name1 thread-name2
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

    def _print_coroutines_list(self, coroutines, current_coroutine_id):
        gdb.write(" Id\tState\t\terrno\tFunction\n")

        for coro in coroutines:
            coro_id = coro[MM_COROUTINE_ID_FIELD_NAME]
            coro_state = coro[MM_COROUTINE_STATE_FIELD_NAME]
            coro_errno = coro[MM_COROUTINE_ERRNO_FIELD_NAME]
            coro_func = coro[MM_COROUTINE_FUNCTION_FIELD_NAME]
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

        coroutines = mm_current_thread_coroutines()
        current_coroutine_id = mm_get_current_thread_coroutine_id()

        self._print_coroutines_list(coroutines, current_coroutine_id)

    def invoke(self, args, is_tty):
        with gdb_thread_restore():
            for thread in self._get_threads_list_from_args(args):
                self._list_coroutines_for_thread(thread)
                gdb.write("\n")


class MMCoroutineCmd(gdb.Command):
    """Execute gdb command in the context of machinarium coroutine.
Or just show the object of it.
Please note that coroutine is determined by the pair of
thread name or thread id and coroutine id.

Usage: (gdb) mmcoro <thread_id> <coro_id> <gdbcmd?>

Example:
    (gdb) mmcoro thread-name 42
    (gdb) mmcoro thread-name 42 info stack
    """

    def __init__(self):
        super(MMCoroutineCmd, self).__init__(
            "mmcoro", gdb.COMMAND_STACK, gdb.COMPLETE_NONE)

    def _parse_args(self, args):
        thread_id, coro_id, *gdbcmd = gdb.string_to_argv(args)

        return thread_id, parse_int_or_none(coro_id), ' '.join(gdbcmd)

    def _execute_in_coroutine_context(self, coroutine, gdbcmd):
        # There is no need to change context for current coroutines - it is already
        # equal to the context of current frame
        # More over, there is no way to change context for current coroutines
        # because of the coroutine->context is not valid for currently running coros
        # (sp may be change because of functions call)
        current_coroutine_id = mm_get_current_thread_coroutine_id()
        if coroutine[MM_COROUTINE_ID_FIELD_NAME] == current_coroutine_id:
            with gdb_frame_restore():
                gdb.execute(gdbcmd)
            return

        with gdb_frame_restore():
            pc, sp = mm_get_pc_sp_for_coroutine_x64(coroutine)
            with gdb_pc_sp_set_and_restore(pc, sp):
                gdb.execute(gdbcmd)

    def invoke(self, args, is_tty):
        if gdb_get_current_platform() != 'i386:x86-64':
            # To perform such things in other platforms
            # mm_get_pc_and_sp_for_coroutine_x64 should be rewritten for desired platform
            gdb.write(f'Current platform is not supported for this command.\n')
            return

        thread_id, coro_id, gdbcmd = self._parse_args(args)
        if thread_id is None or coro_id is None:
            gdb.write(f"Can't parse '{args}', see 'help mmcoro' for more\n")
            return

        with gdb_thread_restore():
            thread = mm_find_thread(thread_id)
            if thread is None:
                gdb.write(
                    f"There is no thread '{thread_id}'\n")
                return

            thread.switch()

            coroutine = mm_find_coroutine_in_current_thread(coro_id)
            if coroutine is None:
                gdb.write(
                    f"There is no coroutine {coro_id}\n")
                return

            if len(gdbcmd) == 0:
                gdb.execute(
                    f'print *({MM_COROUTINE_TYPE_NAME}*){coroutine.address}')
                return

            self._execute_in_coroutine_context(coroutine, gdbcmd)


MMCoroutines()
MMCoroutineCmd()


gdb.write('done.\n', stream=gdb.STDLOG)

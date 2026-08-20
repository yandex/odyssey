import gdb
import gdb.unwinder

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
MM_COROUTINE_FUNCTION_ARG_NAME = "function_arg"
MM_COROUTINE_STATE_FIELD_NAME = "state"
MM_COROUTINE_ERRNO_FIELD_NAME = "errno_"
MM_COROUTINE_CONTEXT_FIELD_NAME = "context"
MM_COROUTINE_NAME_FIELD_NAME = "name"
MM_COROUTINE_ALLOCATED_BYTES_FIELD_NAME = "allocated_bytes"
MM_COROUTINE_FREED_BYTES_FIELD_NAME = "freed_bytes"

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

# see hm.h
MM_HASHMAP_TYPE_NAME = "mm_hashmap_t"
MM_HASHMAP_KVP_TYPE_NAME = "mm_hashmap_kvp_t"
MM_HASHMAP_BUCKET_TYPE_NAME = "mm_hashmap_bucket_t"
MM_HASHMAP_KVP_LINK_FIELD_NAME = "link"
MM_HASHMAP_KVP_HASH_FIELD_NAME = "hash"

# see pstmt.h
OD_PSTMT_TYPE_NAME = "od_pstmt_t"
OD_PSTMT_DESC_TYPE_NAME = "od_pstmt_desc_t"
OD_PSTMT_DESC_DATA_FIELD_NAME = "data"
OD_PSTMT_DESC_LEN_FIELD_NAME = "len"
OD_PSTMT_DESC_FIELD_NAME = "desc"
OD_PSTMT_NAME_FIELD_NAME = "name"
OD_PSTMT_REFS_FIELD_NAME = "refs"

OD_CLIENT_SERVER_FIELD_NAME = "server"

OD_SERVER_ID_FIELD_NAME = "id"
OD_SERVER_STATE_FIELD_NAME = "state"

GDB_CHAR_POINTER_TYPE = gdb.lookup_type("char").pointer()
GDB_MM_COROUTINE_TYPE = gdb.lookup_type(MM_COROUTINE_TYPE_NAME)
GDB_MM_COROUTINE_POINTER_TYPE = GDB_MM_COROUTINE_TYPE.pointer()

GDB_OD_CLIENT_PTR_TYPE = gdb.lookup_type("od_client_t").pointer()
GDB_OD_SERVER_PTR_TYPE = gdb.lookup_type("od_server_t").pointer()

GDB_OD_LIST_TYPE = gdb.lookup_type('od_list_t')
GDB_OD_LIST_POINTER_TYPE = GDB_OD_LIST_TYPE.pointer()

GDB_MM_HASHMAP_TYPE = gdb.lookup_type(MM_HASHMAP_TYPE_NAME)
GDB_MM_HASHMAP_POINTER_TYPE = GDB_MM_HASHMAP_TYPE.pointer()
GDB_MM_HASHMAP_KVP_TYPE = gdb.lookup_type(MM_HASHMAP_KVP_TYPE_NAME)
GDB_MM_HASHMAP_KVP_POINTER_TYPE = GDB_MM_HASHMAP_KVP_TYPE.pointer()
GDB_MM_HASHMAP_BUCKET_TYPE = gdb.lookup_type(MM_HASHMAP_BUCKET_TYPE_NAME)

GDB_OD_PSTMT_TYPE = gdb.lookup_type(OD_PSTMT_TYPE_NAME)
GDB_OD_PSTMT_POINTER_TYPE = GDB_OD_PSTMT_TYPE.pointer()
GDB_OD_PSTMT_DESC_TYPE = gdb.lookup_type(OD_PSTMT_DESC_TYPE_NAME)

GDB_UINT64_TYPE = gdb.lookup_type('uint64_t')
GDB_SIZE_T_TYPE = gdb.lookup_type('size_t')


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


def mm_get_field_offset(element_type, field_name):
    for f in element_type.fields():
        if f.name == field_name:
            return f.bitpos // 8

    raise KeyError(
        f'{field_name} field not found in type {element_type}')


def get_mm_coroutine_link_offset():
    return mm_get_field_offset(GDB_MM_COROUTINE_TYPE, MM_COROUTINE_LINK_FIELD_NAME)


MM_COROUTINE_LINK_FIELD_OFFSET = get_mm_coroutine_link_offset()


def gdb_get_current_platform():
    try:
        return gdb.selected_frame().architecture().name()
    except gdb.error:
        return None


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


def mm_current_thread_coroutines(fn_name_filter: str = ''):
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

    all_coros = active_coroutines + ready_coroutines

    if len(fn_name_filter) > 0:
        filtered = []
        fn = gdb.parse_and_eval(f'&{fn_name_filter}')
        for c in all_coros:
            if fn == c[MM_COROUTINE_FUNCTION_FIELD_NAME]:
                filtered.append(c)

        return filtered

    return all_coros


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


def mm_get_thread_coroutines(thread):
    thread.switch()

    mm_self_ptr = get_mm_self_or_none()
    if mm_self_ptr is None:
        return []

    if mm_self_ptr == 0:
        return []

    return mm_current_thread_coroutines()


def format_bytes(bytes_count):
    if bytes_count < 1024:
        return f'{bytes_count}B'
    elif bytes_count < 1024 * 1024:
        return f'{bytes_count / 1024:.2f}KB'
    elif bytes_count < 1024 * 1024 * 1024:
        return f'{bytes_count / (1024 * 1024):.2f}MB'
    else:
        return f'{bytes_count / (1024 * 1024 * 1024):.2f}GB'


def mm_get_context_registers_for_coroutine_x64(coroutine: gdb.Value):
    context = coroutine[MM_COROUTINE_CONTEXT_FIELD_NAME]
    raw_sp = context[MM_CONTEXT_SP_FIELD_NAME]

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

    reg_ptr = raw_sp
    r15 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    r14 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    r13 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    r12 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    rbx = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    rbp = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    rip = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    rsp = reg_ptr

    return {
        'rsp': rsp,

        'rip': rip,

        'r15': r15,
        'r14': r14,
        'r13': r13,
        'r12': r12,
        'rbx': rbx,
        'rbp': rbp,
    }


def mm_get_context_registers_for_coroutine_aarch64(coroutine: gdb.Value):
    context = coroutine[MM_COROUTINE_CONTEXT_FIELD_NAME]
    raw_sp = context[MM_CONTEXT_SP_FIELD_NAME]

    # There is the stack (raw_sp) 'inside' mm_context_switch.
    # See context_swap_aarch64.S: registers are saved in pairs with
    # stp Xn, Xm, [sp, #-16]! (pre-decrement). The stack is LIFO,
    # so the LAST pair pushed (x29, x30) ends up at the LOWEST address.
    #
    # low addr |   x29 (fp)            <- raw_sp
    #          |   x30 (lr)            <- desired pc
    #          |   x27
    #          |   x28
    #          |   x25
    #          |   x26
    #          |   x23
    #          |   x24
    #          |   x21
    #          |   x22
    #          |   x19
    #          |   x20
    #          |   x17
    #          |   x18
    #          |   x8
    #          |   x16
    # high addr|   [frame of the coro] <- desired sp

    reg_ptr = raw_sp
    x29 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x30 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x27 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x28 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x25 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x26 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x23 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x24 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x21 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x22 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x19 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x20 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x17 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x18 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x8 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    x16 = gdb.parse_and_eval(f'(uint64_t*)({reg_ptr})').dereference()

    reg_ptr += 1
    sp = reg_ptr

    return {
        'sp': sp,

        'pc': x30,

        'x8': x8,
        'x16': x16,
        'x17': x17,
        'x18': x18,
        'x19': x19,
        'x20': x20,
        'x21': x21,
        'x22': x22,
        'x23': x23,
        'x24': x24,
        'x25': x25,
        'x26': x26,
        'x27': x27,
        'x28': x28,
        'x29': x29,
        'x30': x30,
    }


def mm_get_context_registers_for_coroutine(coroutine: gdb.Value):
    platform = gdb_get_current_platform()

    if platform == 'i386:x86-64':
        return mm_get_context_registers_for_coroutine_x64(coroutine)

    if platform and platform.startswith('aarch64'):
        return mm_get_context_registers_for_coroutine_aarch64(coroutine)

    raise gdb.error(f'unsupported platform: {platform}')


class MMFrameId:
    def __init__(self, sp: gdb.Value, pc: gdb.Value):
        self.sp = sp
        self.pc = pc


class MMContextSelector(gdb.unwinder.Unwinder):
    def __init__(self) -> None:
        super().__init__("mm-unwinder")
        self.registers = None
        gdb.invalidate_cached_frames()

    def target_to(self, registers) -> None:
        self.registers = registers

    def __call__(self, pending_frame: gdb.PendingFrame) -> gdb.UnwindInfo:
        if self.registers is None:
            return None

        sp = self.registers.get('rsp', self.registers.get('sp'))
        pc = self.registers.get('rip', self.registers.get('pc'))

        unwind_info = pending_frame.create_unwind_info(MMFrameId(sp, pc))
        for reg in self.registers:
            unwind_info.add_saved_register(reg, self.registers[reg])

        self.registers = None
        return unwind_info


mm_context_selector = MMContextSelector()
gdb.unwinder.register_unwinder(None, mm_context_selector, replace=True)


class MMCoroutiesFrameFilter:
    def __init__(self) -> None:
        self.name = "mm-coroutines-frame-filter"
        self.enabled = False
        self.priority = 100

        gdb.frame_filters[self.name] = self

    def filter(self, iters):
        p = list(iters)
        if len(p) <= 1:
            return p

        return p[1:]

    @contextmanager
    def enabled_filter(self):
        try:
            self.enabled = True
            yield
        finally:
            self.enabled = False


mm_first_frame_skip = MMCoroutiesFrameFilter()


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
            "info mmcoros", gdb.COMMAND_STACK, gdb.COMPLETE_EXPRESSION)

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
        gdb.write(" Id\tState\t\terrno\tFunction\tArg\tName\tMemstat\n")

        for coro in coroutines:
            coro_id = coro[MM_COROUTINE_ID_FIELD_NAME]
            coro_state = coro[MM_COROUTINE_STATE_FIELD_NAME]
            coro_errno = coro[MM_COROUTINE_ERRNO_FIELD_NAME]
            coro_func = coro[MM_COROUTINE_FUNCTION_FIELD_NAME]
            coro_arg = coro[MM_COROUTINE_FUNCTION_ARG_NAME]
            coro_name = coro[MM_COROUTINE_NAME_FIELD_NAME]
            current_coro_pref = ' ' if coro_id != current_coroutine_id else '*'

            try:
                coro_allocated = int(coro[MM_COROUTINE_ALLOCATED_BYTES_FIELD_NAME])
                coro_freed = int(coro[MM_COROUTINE_FREED_BYTES_FIELD_NAME])
                coro_mem_used = coro_allocated - coro_freed
                memstat = f'{format_bytes(coro_allocated)}/{format_bytes(coro_freed)}/{format_bytes(coro_mem_used)}'
            except (gdb.error, KeyError):
                memstat = '?/?/?'

            gdb.write(
                f'{current_coro_pref}{coro_id}\t{coro_state}\t{coro_errno}\t{coro_func}\t{coro_arg}\t{coro_name}\t{memstat}\n')

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
    (gdb) mmcoro all info stack
    """

    def __init__(self):
        super(MMCoroutineCmd, self).__init__(
            "mmcoro", gdb.COMMAND_STACK, gdb.COMPLETE_EXPRESSION)

    def _parse_specified_coro_args(self, args):
        thread_id, coro_id, *gdbcmd = gdb.string_to_argv(args)

        return thread_id, parse_int_or_none(coro_id), ' '.join(gdbcmd)

    def _parse_args(self, args):
        if len(args) == 0:
            raise gdb.error('expected arguments, see info mmcoro for more')

        if args[0] == 'all' or args[0] == 'a':
            thread_coroutines_list = []
            for th in gdb.selected_inferior().threads():
                coros = mm_get_thread_coroutines(th)
                thread_coroutines_list.append((th, coros))
            return thread_coroutines_list, ' '.join(gdb.string_to_argv(args)[1:])

        thread_id, coro_id, gdbcmd = self._parse_specified_coro_args(args)
        thread = mm_find_thread(thread_id)
        if thread is None:
            gdb.write(
                f"There is no thread '{thread_id}'\n")
            return [], None

        thread.switch()

        coroutine = mm_find_coroutine_in_current_thread(coro_id)
        if coroutine is None:
            gdb.write(
                f"There is no coroutine {coro_id}\n")
            return [], None

        return [(thread, (coroutine, ))], gdbcmd

    def _execute_in_coroutine_context(self, thread: gdb.InferiorThread, coroutine: gdb.Value, gdbcmd: str) -> None:
        if len(gdbcmd) == 0:
            gdb.execute(
                f'print *({MM_COROUTINE_TYPE_NAME}*){coroutine.address}'
            )
            return

        thread.switch()

        gdb.write(
            f'Coroutine {int(coroutine[MM_COROUTINE_ID_FIELD_NAME])}:\n'
        )

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

        # To switch to od_auth_frontend the coroutine context, we can use the frame unwider.
        # However, this will cause the first frame to be the frame of the current thread.
        # And then we unwides to the coroutine context.
        # Therefore, we should use a frame filter to skip the first frame.
        try:
            with mm_first_frame_skip.enabled_filter():
                regs = mm_get_context_registers_for_coroutine(coroutine)
                mm_context_selector.target_to(regs)
                gdb.invalidate_cached_frames()
                gdb.execute(gdbcmd)
        finally:
            gdb.invalidate_cached_frames()

    def invoke(self, args, is_tty):
        platform = gdb_get_current_platform()
        if platform != 'i386:x86-64' and not (platform and platform.startswith('aarch64')):
            gdb.write(
                f"!!! Warning: current platform ({platform}) is not supported for this command !!!\n"
            )

        with gdb_thread_restore():
            gdb.write(
                f"Please be careful with analyzing for the frame with zero index\n"
            )
            gdb.write(
                f"Unless your command is not bt, the zero frame will be printed, and this frame is fake.\n"
            )
            gdb.write("\n")

            thread_coroutines_list, gdbcmd = self._parse_args(args)

            for th_coros in thread_coroutines_list:
                thread, coroutines = th_coros[0], th_coros[1]

                gdb.write(f"Thread {thread.num} ({thread.name}) machinarium coroutines execution:\n")
                for coro in coroutines:
                    self._execute_in_coroutine_context(thread, coro, gdbcmd)


class IgnoreErrorsCmd (gdb.Command):
    """Execute a single command, ignoring all errors.
Only one-line commands are supported.
This is primarily useful in scripts."""

    def __init__(self):
        super(IgnoreErrorsCmd, self).__init__("ignore-errors",
                                              gdb.COMMAND_OBSCURE,
                                              # FIXME...
                                              gdb.COMPLETE_COMMAND)

    def invoke(self, arg, from_tty):
        try:
            gdb.execute(arg, from_tty)
        except:
            pass


class ODGetFieldOffset(gdb.Command):
    """Print field offset in struct. Usage:
    od-get-field-offsset <list-addr> <element-type> <link-field>

Examples:
    (gdb) od-get-field-offsset od_rule_storage_t link
    """

    def __init__(self):
        super(ODGetFieldOffset, self).__init__(
            "od-get-field-offsset", gdb.COMMAND_STACK, gdb.COMPLETE_EXPRESSION)

    def invoke(self, args, _):
        argv = gdb.string_to_argv(args)
        if len(argv) != 2:
            gdb.write(
                'Expected 2 arguments: list element type and link field name\n', stream=gdb.STDLOG)
            return

        element_type, link_field_name = argv

        gdb.write(
            f'{mm_get_field_offset(gdb.lookup_type(element_type), link_field_name)}\n', stream=gdb.STDLOG)


def _od_list_iterate(list_addr, element_type, link_field_name, action):
    element_type = gdb.lookup_type(element_type)
    element_ptr_type = element_type.pointer()
    link_field_offset = mm_get_field_offset(element_type, link_field_name)

    list_addr = gdb.parse_and_eval(
        f'{list_addr}').cast(GDB_OD_LIST_POINTER_TYPE)
    list_val = list_addr.dereference()

    # like od_list_foreach
    iterator = list_val[MM_LIST_NEXT_FIELD_NAME]
    while iterator != list_addr:
        iterator_as_char_ptr = iterator.cast(GDB_CHAR_POINTER_TYPE)
        element_ptr = (iterator_as_char_ptr -
                       link_field_offset).cast(element_ptr_type)
        element_val = element_ptr.dereference()

        action(element_type, element_ptr, element_val)

        iterator = iterator[MM_LIST_NEXT_FIELD_NAME]


class ODListPrint(gdb.Command):
    """Print content of the odyssey list. Usage:
    od-list-print <list-addr> <element-type> <link-field>

Examples:
    (gdb) od-list-print &rules->storages od_rule_storage_t link
    """

    def __init__(self):
        super(ODListPrint, self).__init__(
            "od-list-print", gdb.COMMAND_STACK, gdb.COMPLETE_EXPRESSION)

    def invoke(self, args, _):
        argv = gdb.string_to_argv(args)
        if len(argv) != 3:
            gdb.write(
                'Expected 3 arguments: od_list_t address, list element type and link field name\n', stream=gdb.STDLOG)
            return

        list_addr, element_type, link_field_name = argv
        total = 0

        def print_element(element_type, element_ptr, element_val):
            nonlocal total
            total += 1
            gdb.write(f"({element_type}*){element_ptr}: {element_val}\n\n",
                      stream=gdb.STDLOG)

        _od_list_iterate(list_addr, element_type,
                         link_field_name, print_element)

        gdb.write(f"Total elements in list: {total}\n", stream=gdb.STDLOG)


class ODListPrintSelect(gdb.Command):
    """Print content of the odyssey list with field selection. Usage:
    od-list-print-select <list-addr> <element-type> <link-field> <select-field>

Examples:
    (gdb) od-list-print-select &rules->storages od_rule_storage_t link name
    """

    def __init__(self):
        super(ODListPrintSelect, self).__init__(
            "od-list-print-select", gdb.COMMAND_STACK, gdb.COMPLETE_EXPRESSION)

    def invoke(self, args, _):
        argv = gdb.string_to_argv(args)
        if len(argv) != 4:
            gdb.write(
                'Expected 4 arguments: od_list_t address, list element type, link field name and selected field name\n', stream=gdb.STDLOG)
            return

        list_addr, element_type, link_field_name, select_field = argv
        total = 0

        def print_element(element_type, element_ptr, element_val):
            nonlocal total
            total += 1
            gdb.write(f"(({element_type}*){element_ptr})->{select_field}: {element_val[select_field]}\n\n",
                      stream=gdb.STDLOG)

        _od_list_iterate(list_addr, element_type,
                         link_field_name, print_element)

        gdb.write(f"Total elements in list: {total}\n", stream=gdb.STDLOG)


class ODClientCoroutines(gdb.Command):
    """List all client coroutines. Usage:
    info clients         - list client coroutines for all threads
    info clients <id(s)> - list client coroutines for specified thread(s). <id> can be thread id or name.

Examples:
    (gdb) info clients
    (gdb) info clients thread-name1 42
    (gdb) info clients 41 42 43
    (gdb) info clients thread-name1 thread-name2
    """

    def __init__(self):
        super(ODClientCoroutines, self).__init__(
            "info clients", gdb.COMMAND_STACK, gdb.COMPLETE_EXPRESSION)

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
        gdb.write(" Id\tState\t\terrno\tFunction\tArg\tName\tMemstat\n")

        for coro in coroutines:
            coro_id = coro[MM_COROUTINE_ID_FIELD_NAME]
            coro_state = coro[MM_COROUTINE_STATE_FIELD_NAME]
            coro_errno = coro[MM_COROUTINE_ERRNO_FIELD_NAME]
            coro_func = coro[MM_COROUTINE_FUNCTION_FIELD_NAME]
            coro_arg = coro[MM_COROUTINE_FUNCTION_ARG_NAME]
            coro_name = coro[MM_COROUTINE_NAME_FIELD_NAME]

            try:
                coro_allocated = int(coro[MM_COROUTINE_ALLOCATED_BYTES_FIELD_NAME])
                coro_freed = int(coro[MM_COROUTINE_FREED_BYTES_FIELD_NAME])
                coro_mem_used = coro_allocated - coro_freed
                memstat = f'{format_bytes(coro_allocated)}/{format_bytes(coro_freed)}/{format_bytes(coro_mem_used)}'
            except (gdb.error, KeyError):
                memstat = '?/?/?'

            current_coro_pref = ' ' if coro_id != current_coroutine_id else '*'

            gdb.write(
                f'{current_coro_pref}{coro_id}\t{coro_state}\t{coro_errno}\t{coro_func}\t{coro_arg}\t{coro_name}\t{memstat}\n')

    def _list_coroutines_for_thread(self, thread):
        thread.switch()

        gdb.write(
            f"Thread {thread.num} ({thread.name}) client coroutines:\n")

        mm_self_ptr = get_mm_self_or_none()
        if mm_self_ptr is None:
            gdb.write(
                f" There is no {MM_SELF_VARIABLE_NAME} in the current context. Does the executable actually use the machinarium framework?\n")
            return

        if mm_self_ptr == 0:
            gdb.write(
                f" The {MM_SELF_VARIABLE_NAME} is NULL, so no coroutines in this thread available.\n")
            return

        coroutines = mm_current_thread_coroutines('od_frontend')
        current_coroutine_id = mm_get_current_thread_coroutine_id()

        self._print_coroutines_list(coroutines, current_coroutine_id)

    def invoke(self, args, is_tty):
        with gdb_thread_restore():
            for thread in self._get_threads_list_from_args(args):
                self._list_coroutines_for_thread(thread)
                gdb.write("\n")


class ODListCurrentServers(gdb.Command):
    """List all servers accessible from any clients. Usage:
    info servers         - list servers from client coroutines for all threads
    info servers <id(s)> - list servers from client coroutines for specified thread(s). <id> can be thread id or name.

Examples:
    (gdb) info servers
    (gdb) info servers thread-name1 42
    (gdb) info servers 41 42 43
    (gdb) info servers thread-name1 thread-name2
    """

    def __init__(self):
        super(ODListCurrentServers, self).__init__(
            "info servers", gdb.COMMAND_STACK, gdb.COMPLETE_EXPRESSION)

    def _get_threads_list_from_args(self, args):
        ids = set(gdb.string_to_argv(args))

        if len(ids) == 0:
            return gdb.selected_inferior().threads()

        threads_list = []
        for thr in gdb.selected_inferior().threads():
            if thr.name in ids or str(thr.num) in ids:
                threads_list.append(thr)

        return threads_list

    def _print_servers_list(self, coroutines):
        gdb.write("cl-id\tsrv-id\tptr\tstate\n")

        for coro in coroutines:
            client_struct_ptr = coro[MM_COROUTINE_FUNCTION_ARG_NAME]
            client_struct_ptr = client_struct_ptr.cast(GDB_OD_CLIENT_PTR_TYPE)
            server_struct_ptr = client_struct_ptr.dereference(
            )[OD_CLIENT_SERVER_FIELD_NAME].cast(GDB_OD_SERVER_PTR_TYPE)
            client_id = client_struct_ptr.dereference()[
                OD_SERVER_ID_FIELD_NAME]['id']

            if server_struct_ptr == 0:
                gdb.write(f'{client_id}\tUNDEF\tUNDEF\tUNDEF\n')
                continue

            server = server_struct_ptr.dereference()
            server_id = server[OD_SERVER_ID_FIELD_NAME]['id']
            server_state = server[OD_SERVER_STATE_FIELD_NAME]

            gdb.write(f'{client_id}\t{server_id}\t{server_struct_ptr}\t{server_state}\n')

    def _list_coroutines_for_thread(self, thread):
        thread.switch()

        gdb.write(
            f"Thread {thread.num} ({thread.name}) client coroutines:\n")

        mm_self_ptr = get_mm_self_or_none()
        if mm_self_ptr is None:
            gdb.write(
                f" There is no {MM_SELF_VARIABLE_NAME} in the current context. Does the executable actually use the machinarium framework?\n")
            return

        if mm_self_ptr == 0:
            gdb.write(
                f" The {MM_SELF_VARIABLE_NAME} is NULL, so no coroutines in this thread available.\n")
            return

        coroutines = mm_current_thread_coroutines('od_frontend')

        self._print_servers_list(coroutines)

    def invoke(self, args, is_tty):
        with gdb_thread_restore():
            for thread in self._get_threads_list_from_args(args):
                self._list_coroutines_for_thread(thread)
                gdb.write("\n")


class ODHashmapPrint(gdb.Command):
    """Print content of an Odyssey mm_hashmap_t. Usage:
    od-hashmap-print <hashmap-expr> [max-entries]

    The command auto-detects the key/value layout based on the hashmap
    configuration (keysz/valsz):

    - Client/server/portals maps:
        key   = char *            (prepared statement / portal name)
        value = od_pstmt_t *      (pointer)

    - Global prepared statements map:
        key   = od_pstmt_desc_t   { void *data; size_t len; }
        value = od_pstmt_t        (stored inline)

    Examples:
        (gdb) od-hashmap-print client->prep_stmt_ids
        (gdb) od-hashmap-print client->portals
        (gdb) od-hashmap-print server->prep_stmts
        (gdb) od-hashmap-print gm->hm
        (gdb) od-hashmap-print gm->hm 1000
        (gdb) od-hashmap-print 0x60351b012345
    """

    def __init__(self):
        super(ODHashmapPrint, self).__init__(
            "od-hashmap-print", gdb.COMMAND_DATA, gdb.COMPLETE_EXPRESSION)

    def _read_ptr_at(self, addr):
        addr = int(addr)
        return int(gdb.parse_and_eval(f'(void **){hex(addr)}').dereference())

    def _read_size_t_at(self, addr):
        addr = int(addr)
        return int(
            gdb.parse_and_eval(
                f'({GDB_SIZE_T_TYPE.tag}*){hex(addr)}'
            ).dereference()
        )

    def _read_uint64_at(self, addr):
        addr = int(addr)
        return int(
            gdb.parse_and_eval(
                f'({GDB_UINT64_TYPE.tag}*){hex(addr)}'
            ).dereference()
        )

    def _read_cstring(self, addr):
        try:
            return gdb.parse_and_eval(
                f'(char *){hex(int(addr))}'
            ).string()
        except gdb.error:
            return None

    def _detect_kind(self, hm_val):
        """Detect hashmap kind based on keysz/valsz.

        Returns one of: 'global', 'ptr_key_ptr_val'.

        - global: key=od_pstmt_desc_t (16B), value=od_pstmt_t (inline)
        - ptr_key_ptr_val: key=char*, value=od_pstmt_t* (both 8B)
        """
        keysz = int(hm_val['keysz'])
        valsz = int(hm_val['valsz'])
        ptr_sz = int(gdb.parse_and_eval('sizeof(void *)'))

        if keysz == int(GDB_OD_PSTMT_DESC_TYPE.sizeof) and \
                valsz == int(GDB_OD_PSTMT_TYPE.sizeof):
            return 'global'

        if keysz == ptr_sz and valsz == ptr_sz:
            return 'ptr_key_ptr_val'

        raise gdb.error(
            f"unsupported hashmap layout: keysz={keysz} valsz={valsz} "
            f"(ptr_sz={ptr_sz})"
        )

    def _kvp_iterate(self, hm_val):
        """Yield (bucket_index, kvp_addr, hash) for each kvp in the hashmap.

        mm_hashmap_kvp_t layout:
            { mm_list_t link; mm_hash_t hash; uint8_t keyval[]; }
        link is the first field, so kvp == &link.
        """
        link_offset = mm_get_field_offset(
            GDB_MM_HASHMAP_KVP_TYPE, MM_HASHMAP_KVP_LINK_FIELD_NAME)
        hash_offset = mm_get_field_offset(
            GDB_MM_HASHMAP_KVP_TYPE, MM_HASHMAP_KVP_HASH_FIELD_NAME)

        nbuckets = int(hm_val['nbuckets'])
        buckets = hm_val['buckets']

        for bi in range(nbuckets):
            bucket = (buckets + bi).dereference()
            head = bucket['kvps'].address  # mm_list_t sentinel
            head_addr = int(head)
            cur = int(head['next'])
            while cur != 0 and cur != head_addr:
                kvp_addr = cur - link_offset
                hash_val = int(
                    gdb.parse_and_eval(
                        f'({GDB_UINT64_TYPE.tag}*){hex(kvp_addr + hash_offset)}'
                    ).dereference()
                )
                yield bi, kvp_addr, hash_val
                cur = int(
                    gdb.parse_and_eval(
                        f'(mm_list_t *){hex(cur)}'
                    ).dereference()['next']
                )

    def _print_global_entry(self, idx, bi, hash_val, kvp_addr, keyoff, valoff):
        # key: od_pstmt_desc_t { void *data; size_t len; }
        key_addr = kvp_addr + keyoff
        desc_data = self._read_ptr_at(key_addr)
        desc_len = self._read_size_t_at(key_addr + int(
            gdb.parse_and_eval('sizeof(void *)')))

        # value: od_pstmt_t inline
        val_addr = kvp_addr + valoff
        pstmt_val = gdb.parse_and_eval(
            f'({OD_PSTMT_TYPE_NAME} *){hex(val_addr)}'
        ).dereference()

        pstmt_name = str(pstmt_val[OD_PSTMT_NAME_FIELD_NAME].string())
        try:
            refs = int(pstmt_val[OD_PSTMT_REFS_FIELD_NAME])
        except (gdb.error, KeyError):
            refs = -1

        pstmt_desc = pstmt_val[OD_PSTMT_DESC_FIELD_NAME]
        pstmt_desc_data = int(pstmt_desc[OD_PSTMT_DESC_DATA_FIELD_NAME])
        pstmt_desc_len = int(pstmt_desc[OD_PSTMT_DESC_LEN_FIELD_NAME])

        # try to render key data as SQL string
        sql = None
        if desc_data != 0 and desc_len > 0:
            try:
                sql = gdb.parse_and_eval(
                    f'(char *){hex(desc_data)}'
                ).string()
            except gdb.error:
                sql = None

        gdb.write(
            f"[{idx}] bucket={bi} hash={hash_val:#x} "
            f"key={{data={hex(desc_data)}, len={desc_len}}} "
            f"val={{name=\"{pstmt_name}\", "
            f"desc.data={hex(pstmt_desc_data)}, "
            f"desc.len={pstmt_desc_len}, refs={refs}}}"
        )
        if sql is not None:
            gdb.write(f" sql=\"{sql}\"")
        gdb.write("\n")

    def _print_ptr_entry(self, idx, bi, hash_val, kvp_addr, keyoff, valoff):
        # key: char * (C string)
        key_addr = kvp_addr + keyoff
        key_ptr = self._read_ptr_at(key_addr)
        key_str = self._read_cstring(key_ptr) or "<unreadable>"

        # value: od_pstmt_t *
        val_addr = kvp_addr + valoff
        pstmt_ptr = self._read_ptr_at(val_addr)

        if pstmt_ptr != 0:
            pstmt_val = gdb.parse_and_eval(
                f'({OD_PSTMT_TYPE_NAME} *){hex(pstmt_ptr)}'
            ).dereference()
            try:
                pstmt_name = str(pstmt_val[OD_PSTMT_NAME_FIELD_NAME].string())
            except gdb.error:
                pstmt_name = "<unreadable>"
            try:
                refs = int(pstmt_val[OD_PSTMT_REFS_FIELD_NAME])
            except (gdb.error, KeyError):
                refs = -1
            gdb.write(
                f"[{idx}] bucket={bi} hash={hash_val:#x} "
                f"key=\"{key_str}\" "
                f"val->{{name=\"{pstmt_name}\", "
                f"refs={refs}, @ {hex(pstmt_ptr)}}}\n"
            )
        else:
            gdb.write(
                f"[{idx}] bucket={bi} hash={hash_val:#x} "
                f"key=\"{key_str}\" val=NULL\n"
            )

    def invoke(self, args, _):
        argv = gdb.string_to_argv(args)
        if len(argv) < 1:
            gdb.write(
                'Expected 1-2 arguments: mm_hashmap_t* expression '
                'and optional max-entries limit\n', stream=gdb.STDLOG)
            return

        expr = argv[0]
        limit = int(argv[1]) if len(argv) > 1 else 10 ** 9

        ptr = gdb.parse_and_eval(expr)
        if ptr.type.code != gdb.TYPE_CODE_PTR:
            # bare address / non-pointer expression — cast to mm_hashmap_t*
            ptr = ptr.cast(GDB_MM_HASHMAP_POINTER_TYPE)
        if int(ptr) == 0:
            gdb.write("hashmap pointer is NULL\n", stream=gdb.STDLOG)
            return
        hm_val = ptr.dereference()

        kind = self._detect_kind(hm_val)
        keyoff = int(hm_val['keyoff'])
        valoff = int(hm_val['valoff'])
        kvpsize = int(hm_val['kvpsize'])
        nbuckets = int(hm_val['nbuckets'])
        hm_addr = int(hm_val.address) if hm_val.address is not None else 0

        gdb.write(
            f"mm_hashmap_t @ {hex(hm_addr)}: "
            f"kind={kind} nbuckets={nbuckets} kvpsize={kvpsize} "
            f"keyoff={keyoff} valoff={valoff}\n"
        )

        count = 0
        for bi, kvp_addr, hash_val in self._kvp_iterate(hm_val):
            if kind == 'global':
                self._print_global_entry(
                    count, bi, hash_val, kvp_addr, keyoff, valoff)
            else:
                self._print_ptr_entry(
                    count, bi, hash_val, kvp_addr, keyoff, valoff)

            count += 1
            if count >= limit:
                gdb.write(f"... stopped after {limit} entries\n")
                return

        gdb.write(f"total entries: {count}\n")


MMCoroutines()
MMCoroutineCmd()
IgnoreErrorsCmd()
ODListPrint()
ODListPrintSelect()
ODGetFieldOffset()
ODClientCoroutines()
ODListCurrentServers()
ODHashmapPrint()

gdb.write('done.\n', stream=gdb.STDLOG)

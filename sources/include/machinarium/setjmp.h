#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <setjmp.h>

/*
 * Non-local jump that does not save/restore the signal mask.
 *
 * Plain setjmp() is allowed by POSIX to save the signal mask, and on
 * macOS/BSD it does (sigprocmask + sigaltstack syscalls on every call),
 * which is pure overhead on hot paths that use setjmp() only as an error
 * unwind point (e.g. the SQL parsers). glibc's setjmp() already behaves
 * like sigsetjmp(env, 0).
 *
 * MM_HAVE_SIGSETJMP is set by cmake (check_symbol_exists).
 */

#ifdef MM_HAVE_SIGSETJMP

typedef sigjmp_buf mm_jmp_buf_t;

#define mm_setjmp(env) sigsetjmp((env), 0)
#define mm_longjmp(env, val) siglongjmp((env), (val))

#else

typedef jmp_buf mm_jmp_buf_t;

#define mm_setjmp(env) setjmp((env))
#define mm_longjmp(env, val) longjmp((env), (val))

#endif

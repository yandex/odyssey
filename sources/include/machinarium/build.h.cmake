#ifndef MM_BUILD_H_
#define MM_BUILD_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/* AUTO-GENERATED (see build.h.cmake) */

#cmakedefine HAVE_VALGRIND 1
#cmakedefine USE_BORINGSSL 1

#cmakedefine HAVE_ASAN @HAVE_ASAN@
#cmakedefine HAVE_TSAN @HAVE_TSAN@

#cmakedefine USE_UCONTEXT

#cmakedefine MM_MEM_PROF

#cmakedefine USE_TCMALLOC
#cmakedefine USE_TCMALLOC_PROFILE

#endif /* MM_BUILD_H */

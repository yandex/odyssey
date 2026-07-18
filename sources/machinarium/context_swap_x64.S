#if defined(__APPLE__)
#define MM_SYM(name) _##name
#else
#define MM_SYM(name) name
#endif

.text
.globl MM_SYM(mm_context_swap_impl)
#if defined(__linux__)
.type mm_context_swap_impl, @function
#endif

MM_SYM(mm_context_swap_impl):
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq (%rsi), %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret

#if defined(__linux__)
.section .note.GNU-stack,"",@progbits
#endif

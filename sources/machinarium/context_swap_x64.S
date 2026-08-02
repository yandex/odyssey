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
    .cfi_startproc

    pushq %rbp
    .cfi_adjust_cfa_offset 8
    .cfi_offset rbp, -16

    pushq %rbx
    .cfi_adjust_cfa_offset 8
    .cfi_offset rbx, -24

    pushq %r12
    .cfi_adjust_cfa_offset 8
    .cfi_offset r12, -32

    pushq %r13
    .cfi_adjust_cfa_offset 8
    .cfi_offset r13, -40

    pushq %r14
    .cfi_adjust_cfa_offset 8
    .cfi_offset r14, -48

    pushq %r15
    .cfi_adjust_cfa_offset 8
    .cfi_offset r15, -56

    movq %rsp, (%rdi)
    movq (%rsi), %rsp

    .cfi_def_cfa rsp, 56
    .cfi_offset r15, -56
    .cfi_offset r14, -48
    .cfi_offset r13, -40
    .cfi_offset r12, -32
    .cfi_offset rbx, -24
    .cfi_offset rbp, -16

    popq %r15
    .cfi_adjust_cfa_offset -8
    .cfi_restore r15

    popq %r14
    .cfi_adjust_cfa_offset -8
    .cfi_restore r14

    popq %r13
    .cfi_adjust_cfa_offset -8
    .cfi_restore r13

    popq %r12
    .cfi_adjust_cfa_offset -8
    .cfi_restore r12

    popq %rbx
    .cfi_adjust_cfa_offset -8
    .cfi_restore rbx

    popq %rbp
    .cfi_adjust_cfa_offset -8
    .cfi_restore rbp

    ret
    .cfi_endproc

#if defined(__linux__)
.section .note.GNU-stack,"",@progbits
#endif

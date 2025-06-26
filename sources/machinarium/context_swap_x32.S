.text
.globl mm_context_swap_impl
.type mm_context_swap_impl, @function

mm_context_swap_impl:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl %esp, (%eax)
    movl (%edx), %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    ret

.section .note.GNU-stack,"",@progbits
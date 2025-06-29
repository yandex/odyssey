.text
.global mm_context_swap_impl

mm_context_swap_impl:
    stp x8, x16, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    mov x3, sp
    str x3, [x0]
    ldr x3, [x1]
    mov sp, x3

    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x8, x16, [sp], #16

    ret x30

.section .note.GNU-stack,"",@progbits
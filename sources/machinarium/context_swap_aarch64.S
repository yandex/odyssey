#if defined(__APPLE__)
#define MM_SYM(name) _##name
#else
#define MM_SYM(name) name
#endif

.text
.global MM_SYM(mm_context_swap_impl)

MM_SYM(mm_context_swap_impl):
    .cfi_startproc

    stp x8, x16, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x8, -16
    .cfi_offset x16, -8

    stp x17, x18, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x17, -32
    .cfi_offset x18, -24

    stp x19, x20, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x19, -48
    .cfi_offset x20, -40

    stp x21, x22, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x21, -64
    .cfi_offset x22, -56

    stp x23, x24, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x23, -80
    .cfi_offset x24, -72

    stp x25, x26, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x25, -96
    .cfi_offset x26, -88

    stp x27, x28, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x27, -112
    .cfi_offset x28, -104

    stp x29, x30, [sp, #-16]!
    .cfi_adjust_cfa_offset 16
    .cfi_offset x29, -128
    .cfi_offset x30, -120

    mov x3, sp
    str x3, [x0]
    ldr x3, [x1]
    mov sp, x3

    .cfi_def_cfa sp, 128
    .cfi_offset x29, -128
    .cfi_offset x30, -120
    .cfi_offset x27, -112
    .cfi_offset x28, -104
    .cfi_offset x25, -96
    .cfi_offset x26, -88
    .cfi_offset x23, -80
    .cfi_offset x24, -72
    .cfi_offset x21, -64
    .cfi_offset x22, -56
    .cfi_offset x19, -48
    .cfi_offset x20, -40
    .cfi_offset x17, -32
    .cfi_offset x18, -24
    .cfi_offset x8,  -16
    .cfi_offset x16, -8

    ldp x29, x30, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x29
    .cfi_restore x30

    ldp x27, x28, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x27
    .cfi_restore x28

    ldp x25, x26, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x25
    .cfi_restore x26

    ldp x23, x24, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x23
    .cfi_restore x24

    ldp x21, x22, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x21
    .cfi_restore x22

    ldp x19, x20, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x19
    .cfi_restore x20

    ldp x17, x18, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x17
    .cfi_restore x18

    ldp x8, x16, [sp], #16
    .cfi_adjust_cfa_offset -16
    .cfi_restore x8
    .cfi_restore x16

    ret x30
    .cfi_endproc

#if defined(__linux__)
.section .note.GNU-stack,"",@progbits
#endif

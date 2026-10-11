.syntax unified
.cpu cortex-a9
.arm

.section .text, "ax", %progbits

.global minemu_enter_user
.type minemu_enter_user, %function

/*
 * r0: user program's A32 entry address
 * r1: aligned top of the user stack
 */
minemu_enter_user:
    /* Disable IRQ and FIQ during the transition. */
    cpsid if

    /* Preserve the entry address while preparing user registers. */
    mov r2, r0

    /* Prepare initial values for the banked user SP and LR. */
    sub sp, sp, #8
    str r1, [sp]

    mov r3, #0
    str r3, [sp, #4]

    /* Load user SP and LR without changing the current SVC mode. */
    mov r3, sp
    ldmia r3, {sp, lr}^

    /* Release the temporary space on the kernel's SVC stack. */
    add sp, sp, #8

    /* Prepare CPSR for A32 user mode through SPSR_svc. */
    mov r3, #0x10
    msr spsr_cxsf, r3

    /* Set the exception-return destination to the user entry. */
    mov lr, r2

    /* Initialize the first four user registers to zero. */
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0

    /* Jump to the entry address and restore CPSR from SPSR. */
    movs pc, lr

.size minemu_enter_user, .-minemu_enter_user
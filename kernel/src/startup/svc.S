.syntax unified
.cpu cortex-a9
.arm

.section .text, "ax", %progbits

.global minemu_svc_trampoline
.type minemu_svc_trampoline, %function
.extern minemu_svc_dispatch

minemu_svc_trampoline:
    /* Allocate the 88-byte frame on the kernel's SVC stack. */
    sub sp, sp, #88

    /* Preserve user registers before using any as scratch registers. */
    stmia sp, {r0-r12}

    /* Save LR_svc: the address of the instruction after svc. */
    str lr, [sp, #52]

    /* Save the user's CPSR, which the CPU placed in SPSR_svc. */
    mrs r1, spsr
    str r1, [sp, #56]

    /* MINEMU_EXCEPTION_SVC is -3. */
    ldr r1, =-3
    str r1, [sp, #60]

    /* Record the address of the svc instruction itself. */
    sub r1, lr, #4
    str r1, [sp, #64]

    /* SVC has no data-abort fault information. */
    mov r1, #0
    str r1, [sp, #68]
    str r1, [sp, #72]

    /* Save the banked user-mode SP and LR. */
    add r1, sp, #76
    stmia r1, {sp, lr}^

    /* Clear the reserved field. */
    mov r1, #0
    str r1, [sp, #84]

    /* Pass the frame pointer as the first C argument. */
    mov r0, sp
    bl minemu_svc_dispatch

    /* The dispatcher returns the frame pointer in r0. */
    mov sp, r0

    /* Recover the exception return address. */
    ldr lr, [sp, #52]

    /* Restore SPSR so exception return restores the user's CPSR. */
    ldr r1, [sp, #56]
    msr spsr_cxsf, r1

    /* Restore the banked user-mode SP and LR. */
    add r1, sp, #76
    ldmia r1, {sp, lr}^

    /* Restore registers, including the syscall result in saved r0. */
    ldmia sp, {r0-r12}

    /* Release the frame from the SVC stack. */
    add sp, sp, #88

    /* Resume after svc and restore CPSR from SPSR. */
    movs pc, lr

.size minemu_svc_trampoline, .-minemu_svc_trampoline
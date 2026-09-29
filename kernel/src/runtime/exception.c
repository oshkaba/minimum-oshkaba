#include <stdint.h>

#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/uart.h"

void minemu_fail_stop(void)
{
    for (;;) {
        __asm__ volatile("nop");
    }
}

void minemu_panic(const char *message)
{
    (void)message;
    minemu_fail_stop();
}

__attribute__((weak, noreturn)) void minemu_svc_trampoline(void)
{
    minemu_fail_stop();
}

__attribute__((weak)) struct minemu_trap_frame *minemu_svc_dispatch(
    struct minemu_trap_frame *frame)
{
    (void)frame;
    minemu_fail_stop();
}

__attribute__((weak)) struct minemu_trap_frame *minemu_irq_dispatch(
    struct minemu_trap_frame *frame)
{
    uint32_t source = (uint32_t)frame->exception_id;

    if (source == MINEMU_IRQ_UART0) {
        minemu_uart_irq_handler();
    }

    MINEMU_INTERRUPT->eoi = source;

    return frame;
}

__attribute__((weak)) void minemu_undefined_dispatch(
    struct minemu_trap_frame *frame)
{
    (void)frame;
    minemu_fail_stop();
}

__attribute__((weak)) void minemu_abort_dispatch(
    struct minemu_trap_frame *frame)
{
    (void)frame;
    minemu_fail_stop();
}
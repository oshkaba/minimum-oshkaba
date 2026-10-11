#include <stdarg.h>
#include <stdint.h>

#include "minemu/user_abi.h"

static int ioctl_svc(int fd, int op,
                     uint32_t argument0, uint32_t argument1)
{
    register uint32_t r0 __asm__("r0") = (uint32_t)fd;
    register uint32_t r1 __asm__("r1") = (uint32_t)op;
    register uint32_t r2 __asm__("r2") = argument0;
    register uint32_t r3 __asm__("r3") = argument1;
    register uint32_t r7 __asm__("r7") = MINEMU_SYSCALL_IOCTL;

    __asm__ volatile(
        "svc #0"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r3), "r"(r7)
        : "memory", "cc"
    );

    return (int32_t)r0;
}

int ioctl(int fd, int op, ...)
{
    uint32_t argument0 = 0;
    uint32_t argument1 = 0;
    va_list arguments;

    va_start(arguments, op);

    if ((fd == MINEMU_FD_UART0 || fd == MINEMU_FD_UART1) &&
        (op == MINEMU_OP_UART_READ || op == MINEMU_OP_UART_WRITE)) {
        argument0 = (uint32_t)(uintptr_t)va_arg(arguments, char *);
        argument1 = (uint32_t)va_arg(arguments, int);
    } else if (fd == MINEMU_FD_RNG_DEVICE &&
               op == MINEMU_OP_RNG_GET_DATA) {
        argument0 = (uint32_t)(uintptr_t)va_arg(arguments, unsigned int *);
    } else if ((fd == MINEMU_FD_RNG_DEVICE &&
                op == MINEMU_OP_RNG_SET_SEED) ||
               (fd == MINEMU_FD_TRACE_DEVICE &&
                op == MINEMU_OP_TRACE_EVENT)) {
        argument0 = va_arg(arguments, unsigned int);
    }

    else if ((fd == MINEMU_FD_UART_IN &&
              op == MINEMU_IOCTL_UART_READ) ||
             ((fd == MINEMU_FD_UART_OUT || fd == MINEMU_FD_UART_DIAG) &&
              op == MINEMU_IOCTL_UART_WRITE)) {
        argument0 = (uint32_t)(uintptr_t)va_arg(arguments, void *);
        argument1 = va_arg(arguments, uint32_t);
    } else if (fd == MINEMU_FD_RNG &&
               (op == MINEMU_IOCTL_RNG_NEXT ||
                op == MINEMU_IOCTL_RNG_STATE)) {
        argument0 = (uint32_t)(uintptr_t)va_arg(arguments, uint32_t *);
    } else if ((fd == MINEMU_FD_RNG &&
                op == MINEMU_IOCTL_RNG_SEED) ||
               (fd == MINEMU_FD_TRACE &&
                op == MINEMU_IOCTL_TRACE_EVENT)) {
        argument0 = va_arg(arguments, uint32_t);
    }

    va_end(arguments);

    return ioctl_svc(fd, op, argument0, argument1);
}
#include "minemu/user_abi.h"

void minemu_user_main(void)
{
    char message[] = "hello world\n";
    int length = (int)(sizeof(message) - 1U);

    int result = ioctl(
        MINEMU_FD_UART0,
        MINEMU_OP_UART_WRITE,
        message,
        length
    );

    if (result != length) {
        for (;;) {
            __asm__ volatile("nop");
        }
    }

    /* Pause in user mode, matching the supplied hello fixture. */
    __asm__ volatile("bkpt #0");

    for (;;) {
        __asm__ volatile("nop");
    }
}
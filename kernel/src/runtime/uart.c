#include "minemu/platform.h"
#include "minemu/uart.h"

void minemu_uart_putc(char c)
{
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
    }

    MINEMU_UART0->tx_data = (uint32_t)(unsigned char)c;
}


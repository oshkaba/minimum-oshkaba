#include <stddef.h>
#include <stdint.h>

#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/uart.h"

#define MINEMU_UART_RX_BUFFER_SIZE 256U

static volatile unsigned int rx_head;
static volatile unsigned int rx_tail;
static unsigned char rx_buffer[MINEMU_UART_RX_BUFFER_SIZE];

void minemu_uart_putc(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
    }

    MINEMU_UART0->tx_data = (uint32_t)(unsigned char)c;
}

void minemu_uart_rx_init(void) {
    rx_head = 0U;
    rx_tail = 0U;

    MINEMU_UART0->control |= MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
}

void minemu_uart_irq_handler(void) {
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        unsigned char c = (unsigned char)MINEMU_UART0->rx_data;
        unsigned int next = rx_head + 1U;

        if (next == MINEMU_UART_RX_BUFFER_SIZE) {
            next = 0U;
        }

        if (next != rx_tail) {
            rx_buffer[rx_head] = c;
            rx_head = next;
        }
    }
}

int minemu_uart_getc(char *c) {
    int available = 0;

    if (c == NULL) {
        return 0;
    }

    minemu_irq_disable();

    if (rx_head != rx_tail) {
        *c = (char)rx_buffer[rx_tail];

        rx_tail++;

        if (rx_tail == MINEMU_UART_RX_BUFFER_SIZE) {
            rx_tail = 0U;
        }

        available = 1;
    }

    minemu_irq_enable();

    return available;
}
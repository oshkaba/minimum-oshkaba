#ifndef MINEMU_UART_H
#define MINEMU_UART_H

void minemu_uart_putc(char c);
void minemu_uart_rx_init(void);
void minemu_uart_irq_handler(void);
int minemu_uart_getc(char *c);

#endif
#ifndef MINEMU_CONSOLE_H
#define MINEMU_CONSOLE_H

void minemu_console_putc(char c);
void minemu_console_write(const char *str);
void minemu_console_printf(const char *fmt, ...);

#endif
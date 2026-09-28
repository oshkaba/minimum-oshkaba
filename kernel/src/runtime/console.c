#include <stdarg.h>
#include <stddef.h>

#include "minemu/console.h"
#include "minemu/uart.h"

static void minemu_console_write_unsigned(unsigned int value, unsigned int base)
{
    char buffer[32];
    unsigned int i = 0;

    if (value == 0U) {
        minemu_console_putc('0');
        return;
    }

    while (value != 0U) {
        unsigned int digit = value % base;

        if (digit < 10U) {
            buffer[i] = (char)('0' + digit);
        } else {
            buffer[i] = (char)('a' + (digit - 10U));
        }

        i++;
        value /= base;
    }

    while (i != 0U) {
        i--;
        minemu_console_putc(buffer[i]);
    }
}

static void minemu_console_write_signed(int value)
{
    unsigned int magnitude;

    if (value < 0) {
        minemu_console_putc('-');
        magnitude = 0U - (unsigned int)value;
    } else {
        magnitude = (unsigned int)value;
    }

    minemu_console_write_unsigned(magnitude, 10U);
}

static void minemu_console_vprintf(const char *fmt, va_list args)
{
    while (*fmt != '\0') {
        if (*fmt != '%') {
            minemu_console_putc(*fmt);
            fmt++;
            continue;
        }

        fmt++;

        if (*fmt == '\0') {
            minemu_console_putc('%');
            break;
        }

        switch (*fmt) {
            case 'c':
                minemu_console_putc((char)va_arg(args, int));
                break;

            case 's': {
                const char *str = va_arg(args, const char *);

                if (str == NULL) {
                    minemu_console_write("(null)");
                } else {
                    minemu_console_write(str);
                }

                break;
            }

            case 'd':
                minemu_console_write_signed(va_arg(args, int));
                break;

            case 'u':
                minemu_console_write_unsigned(
                    va_arg(args, unsigned int), 10U);
                break;

            case 'x':
                minemu_console_write_unsigned(
                    va_arg(args, unsigned int), 16U);
                break;

            case '%':
                minemu_console_putc('%');
                break;

            default:
                minemu_console_putc('%');
                minemu_console_putc(*fmt);
                break;
        }

        fmt++;
    }
}

void minemu_console_putc(char c)
{
    minemu_uart_putc(c);
}

void minemu_console_write(const char *str)
{
    while (*str != '\0') {
        minemu_console_putc(*str);
        str++;
    }
}

void minemu_console_printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    minemu_console_vprintf(fmt, args);
    va_end(args);
}
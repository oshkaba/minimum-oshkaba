#include <stddef.h>

#include "minemu/console.h"
#include "minemu/msh.h"
#include "minemu/uart.h"

#define MINEMU_MSH_LINE_MAX 20U

static void minemu_msh_execute(char *line)
{
    char *command = line;
    char *end;

    while (*command == ' ') {
        command++;
    }

    if (*command == '\0') {
        return;
    }

    end = command;

    while (*end != '\0' && *end != ' ') {
        end++;
    }

    if ((end - command) == 4 &&
        command[0] == 'e' &&
        command[1] == 'c' &&
        command[2] == 'h' &&
        command[3] == 'o') {
        char *argument = end;
        int wrote_character = 0;
        int pending_space = 0;

        while (*argument == ' ') {
            argument++;
        }

        while (*argument != '\0') {
            if (*argument == ' ') {
                if (wrote_character) {
                    pending_space = 1;
                }
            } else {
                if (pending_space) {
                    minemu_console_putc(' ');
                    pending_space = 0;
                }

                minemu_console_putc(*argument);
                wrote_character = 1;
            }

            argument++;
        }

        minemu_console_putc('\n');
        return;
    }

    minemu_console_write("command not found: ");

    while (*command != '\0' && *command != ' ') {
        minemu_console_putc(*command);
        command++;
    }

    minemu_console_putc('\n');
}

void minemu_msh_run(void)
{
    char line[MINEMU_MSH_LINE_MAX + 1U];
    size_t length = 0U;
    size_t overflow = 0U;

    minemu_console_write("msh> ");

    for (;;) {
        char c;

        if (!minemu_uart_getc(&c)) {
            continue;
        }

        if (c == '\n') {
            if (overflow == 0U) {
                line[length] = '\0';
                minemu_msh_execute(line);
            }

            length = 0U;
            overflow = 0U;

            minemu_console_write("msh> ");
            continue;
        }

        if (c == '\b' || (unsigned char)c == 0x7fU) {
            if (overflow != 0U) {
                overflow--;
            } else if (length != 0U) {
                length--;
            }

            continue;
        }

        if (length < MINEMU_MSH_LINE_MAX && overflow == 0U) {
            line[length] = c;
            length++;
        } else {
            overflow++;
        }
    }
}
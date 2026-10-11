#include <stddef.h>

#include "minemu/user_abi.h"
#include "msh.h"

#define MINEMU_MSH_LINE_MAX 20U

static void msh_io_failure(void)
{
    for (;;) {
        __asm__ volatile("nop");
    }
}

static void msh_putc(char c)
{
    int result = ioctl(
        MINEMU_FD_UART0,
        MINEMU_OP_UART_WRITE,
        &c,
        1
    );

    if (result != 1) {
        msh_io_failure();
    }
}

static void msh_write(const char *text)
{
    size_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }

    if (length == 0U) {
        return;
    }

    int result = ioctl(
        MINEMU_FD_UART0,
        MINEMU_OP_UART_WRITE,
        (char *)text,
        (int)length
    );

    if (result != (int)length) {
        msh_io_failure();
    }
}

static int msh_getc(char *c)
{
    int result = ioctl(
        MINEMU_FD_UART0,
        MINEMU_OP_UART_READ,
        c,
        1
    );

    if (result < 0) {
        msh_io_failure();
    }

    return result;
}

static void minemu_msh_execute(char *line)
{
    char *command = line;
    char *end;

    /* Ignore ASCII spaces before the command. */
    while (*command == ' ') {
        command++;
    }

    /* Empty and all-space lines produce no command output. */
    if (*command == '\0') {
        return;
    }

    /* Find the end of the first ASCII space-delimited word. */
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

        /*
         * Preserve the HW1 behavior: collapse repeated spaces
         * between words and omit trailing spaces.
         */
        while (*argument != '\0') {
            if (*argument == ' ') {
                if (wrote_character) {
                    pending_space = 1;
                }
            } else {
                if (pending_space) {
                    msh_putc(' ');
                    pending_space = 0;
                }

                msh_putc(*argument);
                wrote_character = 1;
            }

            argument++;
        }

        msh_putc('\n');
        return;
    }

    msh_write("command not found: ");

    while (*command != '\0' && *command != ' ') {
        msh_putc(*command);
        command++;
    }

    msh_putc('\n');
}

void minemu_msh_run(void)
{
    char line[MINEMU_MSH_LINE_MAX + 1U];
    size_t length = 0U;
    size_t overflow = 0U;

    msh_write("msh> ");

    for (;;) {
        char c;

        /* A zero return means no input is currently available. */
        if (msh_getc(&c) == 0) {
            continue;
        }

        /* Only newline terminates a command. */
        if (c == '\n') {
            if (overflow == 0U) {
                line[length] = '\0';
                minemu_msh_execute(line);
            }

            length = 0U;
            overflow = 0U;

            msh_write("msh> ");
            continue;
        }

        /* Both ASCII backspace and DEL remove one input byte. */
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
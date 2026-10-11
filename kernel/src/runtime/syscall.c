#include <stdbool.h>
#include <stdint.h>

#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/user_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR  UINT32_C(0x10)
#define CPSR_THUMB    UINT32_C(0x20)
#define A32_SVC_ZERO  UINT32_C(0xef000000)

/* Check that an entire aligned page lies within physical RAM. */
static bool ram_page(uint32_t address)
{
    return (address & (MINEMU_PAGE_SIZE - 1U)) == 0 &&
           address >= MINEMU_RAM_BASE &&
           address - MINEMU_RAM_BASE <=
               MINEMU_RAM_SIZE - MINEMU_PAGE_SIZE;
}

/* Convert a physical RAM address to its kernel direct-map address. */
static const volatile uint32_t *ram_alias(uint32_t address)
{
    return (const volatile uint32_t *)(uintptr_t)
        (MINEMU_KERNEL_DIRECT_BASE + address - MINEMU_RAM_BASE);
}

/* TTBR0 contains the active page directory's physical address. */
static uint32_t current_directory(void)
{
    uint32_t address;

    __asm__ volatile(
        "mrc p15, 0, %0, c2, c0, 0"
        : "=r"(address)
    );

    return address;
}

/* Validate every page before accessing user memory or devices. */
static bool user_range(uint32_t address, uint32_t length,
                       uint32_t permissions)
{
    if (length == 0) {
        return true;
    }

    /* Reject ranges whose last-byte address would overflow. */
    if (address > UINT32_MAX - (length - 1U)) {
        return false;
    }

    uint32_t directory_address = current_directory();

    if (!ram_page(directory_address)) {
        return false;
    }

    const volatile uint32_t *directory =
        ram_alias(directory_address);

    uint32_t last_page =
        (address + length - 1U) & MINEMU_PTE_PAGE_MASK;
    uint32_t page = address & MINEMU_PTE_PAGE_MASK;

    for (;;) {
        uint32_t pde = directory[page >> 22];
        uint32_t table_address = pde & MINEMU_PDE_PAGE_MASK;

        if ((pde & MINEMU_PDE_VALID) == 0 ||
            (pde & ~MINEMU_PDE_ALLOWED_MASK) != 0 ||
            !ram_page(table_address)) {
            return false;
        }

        uint32_t pte =
            ram_alias(table_address)[(page >> 12) & 1023U];

        uint32_t required =
            MINEMU_PTE_VALID | MINEMU_PTE_USER | permissions;

        if ((pte & required) != required ||
            !ram_page(pte & MINEMU_PTE_PAGE_MASK)) {
            return false;
        }

        if (page == last_page) {
            return true;
        }

        page += MINEMU_PAGE_SIZE;
    }
}

/* Perform a validated UART read or write. */
static int32_t uart_transfer(
    volatile struct minemu_uart_regs *uart,
    bool writing,
    uint32_t address,
    uint32_t count,
    bool legacy)
{
    /*
     * A negative signed int length arrives as an unsigned value
     * larger than INT32_MAX.
     */
    if (count > INT32_MAX ||
        (legacy && count > MINEMU_IOCTL_MAX_TRANSFER) ||
        !user_range(address, count,
                    writing ? MINEMU_PTE_READABLE :
                              MINEMU_PTE_WRITABLE)) {
        return MINEMU_IOCTL_ERROR;
    }

    uint8_t *buffer = (uint8_t *)(uintptr_t)address;
    uint32_t transferred = 0;

    if (writing) {
        while (transferred < count) {
            while ((uart->status &
                    MINEMU_UART_STATUS_TX_READY) == 0) {
            }

            uart->tx_data = buffer[transferred++];
        }
    } else {
        /* Read available input without waiting for future input. */
        while (transferred < count &&
               (uart->status &
                MINEMU_UART_STATUS_RX_READY) != 0) {
            buffer[transferred++] = (uint8_t)uart->rx_data;
        }
    }

    return (int32_t)transferred;
}

/* Store RNG data or, for the existing interface, RNG state. */
static int32_t rng_output(uint32_t address, bool state)
{
    if (!user_range(address, sizeof(uint32_t),
                    MINEMU_PTE_WRITABLE)) {
        return MINEMU_IOCTL_ERROR;
    }

    /* Reading DATA advances the RNG, so validate first. */
    uint32_t value =
        state ? MINEMU_RNG->state : MINEMU_RNG->data;

    uint8_t *output = (uint8_t *)(uintptr_t)address;
    const uint8_t *bytes = (const uint8_t *)&value;

    /* Byte stores also handle a destination spanning two pages. */
    for (uint32_t index = 0; index < sizeof(value); ++index) {
        output[index] = bytes[index];
    }

    return 0;
}

/* Select the device operation using the saved syscall arguments. */
static int32_t ioctl_dispatch(uint32_t fd, uint32_t op,
                              uint32_t argument0,
                              uint32_t argument1)
{
    /* Required assignment interface. */
    if (fd == MINEMU_FD_UART0 || fd == MINEMU_FD_UART1) {
        if (op != MINEMU_OP_UART_READ &&
            op != MINEMU_OP_UART_WRITE) {
            return MINEMU_IOCTL_ERROR;
        }

        return uart_transfer(
            fd == MINEMU_FD_UART0 ? MINEMU_UART0 : MINEMU_UART1,
            op == MINEMU_OP_UART_WRITE,
            argument0,
            argument1,
            false
        );
    }

    if (fd == MINEMU_FD_RNG_DEVICE) {
        if (op == MINEMU_OP_RNG_SET_SEED) {
            MINEMU_RNG->seed = argument0;
            return 0;
        }

        if (op == MINEMU_OP_RNG_GET_DATA) {
            return rng_output(argument0, false);
        }

        return MINEMU_IOCTL_ERROR;
    }

    if (fd == MINEMU_FD_TRACE_DEVICE) {
        if (op != MINEMU_OP_TRACE_EVENT) {
            return MINEMU_IOCTL_ERROR;
        }

        MINEMU_TRACE->event = argument0;
        return 0;
    }

    /*
     * Existing repository interface, preserved for supplied tests.
     * Following the process-basics example, input/output/diagnostic
     * descriptors refer to UART0. The explicit UART1 address above
     * provides the assignment's UART1 access.
     */
    switch (op) {
    case MINEMU_IOCTL_UART_READ:
        if (fd != MINEMU_FD_UART_IN) {
            return MINEMU_IOCTL_ERROR;
        }

        return uart_transfer(
            MINEMU_UART0, false, argument0, argument1, true
        );

    case MINEMU_IOCTL_UART_WRITE:
        if (fd != MINEMU_FD_UART_OUT &&
            fd != MINEMU_FD_UART_DIAG) {
            return MINEMU_IOCTL_ERROR;
        }

        return uart_transfer(
            MINEMU_UART0, true, argument0, argument1, true
        );

    case MINEMU_IOCTL_UART_STATUS:
        if (fd != MINEMU_FD_UART_IN &&
            fd != MINEMU_FD_UART_OUT &&
            fd != MINEMU_FD_UART_DIAG) {
            return MINEMU_IOCTL_ERROR;
        }

        return (int32_t)(
            MINEMU_UART0->status &
            (MINEMU_IOCTL_UART_RX_READY |
             MINEMU_IOCTL_UART_TX_READY)
        );

    case MINEMU_IOCTL_RNG_NEXT:
    case MINEMU_IOCTL_RNG_STATE:
        if (fd != MINEMU_FD_RNG) {
            return MINEMU_IOCTL_ERROR;
        }

        return rng_output(
            argument0, op == MINEMU_IOCTL_RNG_STATE
        );

    case MINEMU_IOCTL_RNG_SEED:
        if (fd != MINEMU_FD_RNG) {
            return MINEMU_IOCTL_ERROR;
        }

        MINEMU_RNG->seed = argument0;
        return 0;

    case MINEMU_IOCTL_TRACE_EVENT:
        if (fd != MINEMU_FD_TRACE) {
            return MINEMU_IOCTL_ERROR;
        }

        MINEMU_TRACE->event = argument0;
        return 0;

    default:
        return MINEMU_IOCTL_ERROR;
    }
}

/* Called by the assembly SVC trampoline. */
struct minemu_trap_frame *minemu_svc_dispatch(
    struct minemu_trap_frame *frame)
{
    if (frame->exception_id != MINEMU_EXCEPTION_SVC ||
        (frame->spsr & CPSR_MODE_MASK) != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0 ||
        (frame->fault_pc & 3U) != 0 ||
        frame->return_lr < 4U ||
        frame->fault_pc != frame->return_lr - 4U ||
        !user_range(
            frame->fault_pc,
            sizeof(uint32_t),
            MINEMU_PTE_READABLE | MINEMU_PTE_EXECUTABLE
        )) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    /* Only read the instruction after validating its memory range. */
    uint32_t instruction =
        *(const uint32_t *)(uintptr_t)frame->fault_pc;

    if (instruction != A32_SVC_ZERO ||
        frame->r[7] != MINEMU_SYSCALL_IOCTL) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    frame->r[0] = (uint32_t)ioctl_dispatch(
        frame->r[0],
        frame->r[1],
        frame->r[2],
        frame->r[3]
    );

    return frame;
}
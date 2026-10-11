#include <stdbool.h>
#include <stdint.h>

#include "minemu/mmu.h"
#include "minemu/page_alloc.h"
#include "minemu/platform.h"

#define RAM_PAGE_COUNT (MINEMU_RAM_SIZE / MINEMU_PAGE_SIZE)
#define BITMAP_WORD_COUNT ((RAM_PAGE_COUNT + 31U) / 32U)

extern uint8_t __minemu_kernel_end[];

static uint32_t allocated[BITMAP_WORD_COUNT];
static uint32_t first_page;
static uint32_t next_page;
static bool initialized;

/* Protect allocator state on this single-core machine; preserve IRQ/FIQ masks. */
static uint32_t interrupt_save(void)
{
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr\n\tcpsid if"
                     : "=r"(cpsr) : : "memory");
    return cpsr;
}

static void interrupt_restore(uint32_t cpsr)
{
    __asm__ volatile("msr cpsr_c, %0" : : "r"(cpsr) : "memory", "cc");
}

static bool page_is_allocated(uint32_t page)
{
    return (allocated[page / 32U] & (UINT32_C(1) << (page % 32U))) != 0;
}

bool minemu_page_allocator_init(void)
{
    uint32_t cpsr = interrupt_save();
    uintptr_t end = (uintptr_t)__minemu_kernel_end;

    /* Keep low boot storage/page tables and the entire kernel reserved. */
    if (initialized ||
        end < MINEMU_KERNEL_DIRECT_BASE + UINT32_C(0x00030000) ||
        end > MINEMU_KERNEL_DIRECT_BASE + MINEMU_KERNEL_DIRECT_SIZE ||
        (end & (MINEMU_PAGE_SIZE - 1U)) != 0) {
        interrupt_restore(cpsr);
        return false;
    }

    first_page = (uint32_t)(end - MINEMU_KERNEL_DIRECT_BASE) / MINEMU_PAGE_SIZE;
    next_page = first_page;

    /* Only pages after the kernel are available to the shared allocator. */
    for (uint32_t word = 0; word < BITMAP_WORD_COUNT; ++word) {
        uint32_t base_page = word * 32U;

        if (base_page + 32U <= first_page) {
            allocated[word] = UINT32_MAX;
        } else if (base_page >= first_page) {
            allocated[word] = 0;
        } else {
            allocated[word] =
                (UINT32_C(1) << (first_page - base_page)) - 1U;
        }
    }

    initialized = true;
    interrupt_restore(cpsr);
    return true;
}

uint32_t minemu_page_alloc(void)
{
    uint32_t cpsr = interrupt_save();

    if (!initialized || first_page == RAM_PAGE_COUNT) {
        interrupt_restore(cpsr);
        return 0;
    }

    uint32_t page = next_page;
    uint32_t remaining = RAM_PAGE_COUNT - first_page;

    while (remaining != 0) {
        if (!page_is_allocated(page)) {
            allocated[page / 32U] |= UINT32_C(1) << (page % 32U);

            next_page = page + 1U;
            if (next_page == RAM_PAGE_COUNT) {
                next_page = first_page;
            }

            uint32_t paddr = MINEMU_RAM_BASE + page * MINEMU_PAGE_SIZE;
            volatile uint32_t *words = (volatile uint32_t *)(uintptr_t)(
                MINEMU_KERNEL_DIRECT_BASE + page * MINEMU_PAGE_SIZE);

            /* Explicitly clear on allocation, including pages being reused. */
            for (uint32_t word = 0; word < MINEMU_PAGE_SIZE / 4U; ++word) {
                words[word] = 0;
            }

            interrupt_restore(cpsr);
            return paddr;
        }

        page++;
        if (page == RAM_PAGE_COUNT) {
            page = first_page;
        }
        remaining--;
    }

    interrupt_restore(cpsr);
    return 0;
}

bool minemu_page_free(uint32_t paddr)
{
    uint32_t cpsr = interrupt_save();

    if (!initialized ||
        (paddr & (MINEMU_PAGE_SIZE - 1U)) != 0 ||
        paddr < MINEMU_RAM_BASE ||
        paddr >= MINEMU_RAM_BASE + MINEMU_RAM_SIZE) {
        interrupt_restore(cpsr);
        return false;
    }

    uint32_t page = (paddr - MINEMU_RAM_BASE) / MINEMU_PAGE_SIZE;

    if (page < first_page || !page_is_allocated(page)) {
        interrupt_restore(cpsr);
        return false;
    }

    allocated[page / 32U] &= ~(UINT32_C(1) << (page % 32U));
    next_page = page;

    interrupt_restore(cpsr);
    return true;
}
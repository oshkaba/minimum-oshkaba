#ifndef MINEMU_PAGE_ALLOC_H
#define MINEMU_PAGE_ALLOC_H

#include <stdbool.h>
#include <stdint.h>

bool minemu_page_allocator_init(void);
uint32_t minemu_page_alloc(void);
bool minemu_page_free(uint32_t paddr);

#endif
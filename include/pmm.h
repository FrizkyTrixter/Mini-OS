// include/pmm.h
#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

/* Initialize the physical‐memory manager.
 * Must be called once, after paging is on (so we can use _kernel_end). */
void pmm_init(void);

/* Allocate a single 4 KiB page. Returns the physical address of the page’s start
 * (must be 4 KiB‐aligned). If no free page is left, returns 0. */
uint32_t pmm_alloc_page(void);

/* Free a previously allocated 4 KiB page given by its physical address.
 * addr MUST have been returned by pmm_alloc_page (and not already freed). */
void pmm_free_page(uint32_t phys_addr);

#endif // PMM_H

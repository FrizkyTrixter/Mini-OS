// src/paging.c
#include <stdint.h>

// 1024 entries × 4 KiB = 4 MiB identity-mapped
#define PAGE_ENTRIES      1024
#define PAGE_PRESENT      0x1
#define PAGE_RW           0x2

// Align these tables on 4 KiB boundaries
static uint32_t page_directory[PAGE_ENTRIES]
    __attribute__((aligned(4096)));
static uint32_t first_page_table[PAGE_ENTRIES]
    __attribute__((aligned(4096)));

void init_paging(void) {
    // 1) Fill the first page table: map 0x00000000–0x003FFFFF identity-mapped
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        first_page_table[i] = (i * 0x1000)    // physical address
                             | PAGE_PRESENT   // present
                             | PAGE_RW;       // read/write
    }

    // 2) Point page_directory[0] at first_page_table
    page_directory[0] = ((uint32_t)first_page_table)
                        | PAGE_PRESENT
                        | PAGE_RW;

    // 3) Mark all other PDEs not present
    for (uint32_t i = 1; i < PAGE_ENTRIES; i++) {
        page_directory[i] = 0;
    }

    // 4) Load CR3 with the address of page_directory
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    // 5) Enable paging (set bit 31 of CR0)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

#include "pmm.h"
#include <string.h>    // for memset

void map_page(uint32_t va, uint32_t pa, uint32_t flags) {
    extern uint32_t page_directory[];
    uint32_t pd_idx = va >> 22;
    uint32_t pt_idx = (va >> 12) & 0x3FF;
    uint32_t *pt;

    /* install 2nd-level page table if missing */
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
        uint32_t new_pt_pa = pmm_alloc_page();
        pt = (uint32_t*)(new_pt_pa);
        memset(pt, 0, 4096);
        page_directory[pd_idx] = new_pt_pa | PAGE_PRESENT | PAGE_RW;
    } else {
        pt = (uint32_t*)(page_directory[pd_idx] & 0xFFFFF000);
    }
    /* map the frame */
    pt[pt_idx] = (pa & 0xFFFFF000) | flags;
}


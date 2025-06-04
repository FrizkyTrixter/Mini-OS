// src/pmm.c
#include <stdint.h>
#include <stddef.h>
#include "pmm.h"

/* We cover only the first 4 MiB of RAM (because paging identity‐maps 0–4 MiB).
 * 4 MiB / 4 KiB = 1024 frames. Keep a 1024‐bit bitmap (128 bytes). */
#define PMM_TOTAL_FRAMES   1024
#define PMM_BITMAP_SIZE    (PMM_TOTAL_FRAMES / 8)  // 1024/8 = 128 bytes

/* Base address of the 4 MiB region */
#define PMM_REGION_START   0x00000000u

/* The “end‐of‐kernel” symbol (set in linker.ld). This is the first free byte
 * after the BSS. We’ll extern it here. */
extern uint8_t _kernel_end;

/* Our frame bitmap (0 = free, 1 = used). Each bit covers one 4 KiB frame. */
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE] __attribute__((aligned(4096)));

/* Helpers to set / clear / test bits in pmm_bitmap. */
static inline void pmm_set_frame(uint32_t frame_idx) {
    pmm_bitmap[ frame_idx / 8 ] |=  (1 << (frame_idx % 8));
}

static inline void pmm_clear_frame(uint32_t frame_idx) {
    pmm_bitmap[ frame_idx / 8 ] &= ~(1 << (frame_idx % 8));
}

static inline int pmm_test_frame(uint32_t frame_idx) {
    return (pmm_bitmap[ frame_idx / 8 ] >> (frame_idx % 8)) & 1;
}

/* Find the first free frame (bit 0). Return its index or –1 if none are free. */
static int pmm_find_first_free(void) {
    for (uint32_t byte = 0; byte < PMM_BITMAP_SIZE; ++byte) {
        if (pmm_bitmap[byte] != 0xFF) {
            /* There is at least one zero bit here. Find which bit. */
            for (uint32_t bit = 0; bit < 8; ++bit) {
                if (!((pmm_bitmap[byte] >> bit) & 1)) {
                    return (int)((byte * 8) + bit);
                }
            }
        }
    }
    return -1;  // no free frames
}

/* pmm_init: reserve all frames up to the kernel’s _kernel_end, mark the rest free. */
void pmm_init(void) {
    /* 1) Zero out the bitmap (all frames = free). */
    for (size_t i = 0; i < PMM_BITMAP_SIZE; ++i) {
        pmm_bitmap[i] = 0x00;
    }

    /* 2) Calculate how many frames from 0 need to be marked “used” up to _kernel_end. */
    /*  _kernel_end is a byte-address; round it up to the next frame boundary. */
    uintptr_t kernel_end_pa = (uintptr_t)&_kernel_end;
    uint32_t first_free_frame = (uint32_t)((kernel_end_pa + 0xFFF) / 0x1000);
    /* Now: frames 0..(first_free_frame-1) are in use by the kernel itself. */
    for (uint32_t f = 0; f < first_free_frame && f < PMM_TOTAL_FRAMES; ++f) {
        pmm_set_frame(f);
    }
    /* 3) Everything from first_free_frame … 1023 remains free. */
}

/* pmm_alloc_page: find a free frame, mark it used, return its physical addr. */
uint32_t pmm_alloc_page(void) {
    int frame_idx = pmm_find_first_free();
    if (frame_idx < 0 || frame_idx >= PMM_TOTAL_FRAMES) {
        return 0;  // no free frames
    }
    pmm_set_frame((uint32_t)frame_idx);
    return (uint32_t)(PMM_REGION_START + (frame_idx * 0x1000));
}

/* pmm_free_page: given a physical address, mark that frame “free” again. */
void pmm_free_page(uint32_t phys_addr) {
    if (phys_addr < PMM_REGION_START || phys_addr >= (PMM_REGION_START + PMM_TOTAL_FRAMES * 0x1000)) {
        return;  // out of range: ignore or panic
    }
    /* Must be page-aligned: */
    if (phys_addr & 0xFFF) {
        return;  // not page-aligned → ignore or panic
    }
    uint32_t frame_idx = (phys_addr - PMM_REGION_START) / 0x1000;
    if (frame_idx < PMM_TOTAL_FRAMES) {
        pmm_clear_frame(frame_idx);
    }
}

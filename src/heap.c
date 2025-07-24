// src/heap.c

#include "heap.h"
#include "console.h"
#include <stddef.h>
#include <stdint.h>

/* 16 MiB of kernel heap, but in its own “.heap” section */
#define HEAP_SIZE_BYTES (16 * 1024 * 1024)

/* 
 * Put this array into the linker’s .heap section so that
 * _kernel_end stops *before* it.
 */
static uint8_t __heap_region[HEAP_SIZE_BYTES]
    __attribute__((section(".heap")));

/* Next free byte in the heap */
static uintptr_t heap_ptr;

/**
 * Initialize the bump‑pointer heap.
 * Call once early in kernel startup.
 */
void heap_init(void) {
    heap_ptr = (uintptr_t)&__heap_region[0];
}

/**
 * Allocate `size` bytes, aligned to `align` (power‑of‑two).
 */
void *kmalloc(size_t size, size_t align) {
    uintptr_t aligned = (heap_ptr + (align - 1)) & ~(align - 1);
    void *ptr = (void *)aligned;
    heap_ptr = aligned + size;
    return ptr;
}

/**
 * How many bytes have been handed out so far.
 */
uint32_t heap_used(void) {
    return (uint32_t)(heap_ptr - (uintptr_t)&__heap_region[0]);
}

/**
 * Print heap stats (base, used, remaining) to the console.
 */
void heap_dump_stats(void) {
    print("HEAP BASE   = 0x");
    print_hex((uint32_t)&__heap_region[0]);
    print("\n");

    print("HEAP USED   = ");
    print_hex(heap_used());
    print(" bytes\n");

    print("HEAP REMAIN = ");
    print_hex(HEAP_SIZE_BYTES - heap_used());
    print(" bytes\n");
}

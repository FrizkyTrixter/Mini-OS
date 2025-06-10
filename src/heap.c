// src/heap.c

#include "heap.h"
#include "console.h"
#include <stddef.h>
#include <stdint.h>

/* 16 MiB of kernel heap in static memory (will live in .bss) */
#define HEAP_SIZE_BYTES (16 * 1024 * 1024)

static uint8_t __heap_region[HEAP_SIZE_BYTES];
static uintptr_t heap_ptr;

/**
 * Initialize the bump‐pointer heap.
 * Call this once early in kernel startup.
 */
void heap_init(void) {
    heap_ptr = (uintptr_t)&__heap_region[0];
}

/**
 * Allocate `size` bytes with `align` alignment.
 * `align` must be a power‐of‐two.
 * Returns NULL on out‐of‐memory.
 */
void *kmalloc(size_t size, size_t align) {
    uintptr_t ptr = (heap_ptr + (align - 1)) & ~(uintptr_t)(align - 1);
    if (ptr + size > (uintptr_t)&__heap_region[HEAP_SIZE_BYTES]) {
        return NULL;  // out of heap
    }
    heap_ptr = ptr + size;
    return (void *)ptr;
}

/**
 * Return how many bytes have been allocated so far.
 */
uint32_t heap_used(void) {
    return (uint32_t)(heap_ptr - (uintptr_t)&__heap_region[0]);
}

/**
 * Dump heap stats to the screen via console:
 *   HEAP BASE   = 0x________
 *   HEAP USED   = _____ bytes
 *   HEAP REMAIN = _____ bytes
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

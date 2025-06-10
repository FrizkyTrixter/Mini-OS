#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

/* Initialize the bump‐pointer heap */
void heap_init(void);

/* Allocate `size` bytes aligned to `align` (power‐of‐two) */
void *kmalloc(size_t size, size_t align);

/* --- NEW STATS API --- */

/* How many bytes have been handed out so far */
uint32_t heap_used(void);

/* Print heap base, used, and remaining via console prints */
void heap_dump_stats(void);

#endif // HEAP_H

#ifndef GDT_H
#define GDT_H
#include <stdint.h>

void gdt_init(void);

/* symbol defined in gdt_flush.asm */
extern void gdt_flush(uint32_t gdt_ptr);

#endif


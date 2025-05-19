#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* --- A single GDT entry (packed) --- */
struct gdt_entry {
    uint16_t limit_low;      /* Lower 16 bits of limit */
    uint16_t base_low;       /* Lower 16 bits of base */
    uint8_t  base_middle;    /* Next 8 bits of base */
    uint8_t  access;         /* Access flags */
    uint8_t  granularity;    /* Granularity + high 4 bits of limit */
    uint8_t  base_high;      /* High 8 bits of base */
} __attribute__((packed));

/* --- GDTR pointer format for LGDT --- */
struct gdt_ptr {
    uint16_t limit;          /* Size of GDT minus 1 */
    uint32_t base;           /* Address of first gdt_entry */
} __attribute__((packed));

/* Initialize and load our GDT */
void gdt_init(void);

/* Assembly stub to actually load the GDTR: */
extern void gdt_flush(uint32_t gdt_ptr_address);

#endif /* GDT_H */


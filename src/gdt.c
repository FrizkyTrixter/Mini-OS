/* File: src/gdt.c */

#include <stdint.h>
#include <stddef.h>
#include <gdt.h>      // use the public header in include/, not the local src/gdt.h

/* Define the GDT array and its pointer */
struct gdt_entry gdt[3];
struct gdt_ptr   gp;

/* Assembly stub that actually loads the GDTR */
extern void gdt_flush(uint32_t);

/* Helper to set one GDT descriptor */
static void gdt_set_gate(int idx,
                         uint32_t base,
                         uint32_t limit,
                         uint8_t access,
                         uint8_t gran) {
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));

    gdt[idx].access      = access;
}

/* Build and load the GDT */
void gdt_init(void) {
    /* Point GDTR at our table */
    gp.limit = (uint16_t)(sizeof(gdt) - 1);
    gp.base  = (uint32_t)&gdt;

    /* Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);
    /* Code segment: base=0, limit=4GB, access=0x9A, gran=0xCF */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    /* Data segment: base=0, limit=4GB, access=0x92, gran=0xCF */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Flush into CPU */
    gdt_flush((uint32_t)&gp);
}


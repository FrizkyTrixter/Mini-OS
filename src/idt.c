/* File: src/idt.c */

#include "idt.h"
#include <stdint.h>

/* The IDT itself and the pointer loaded by lidt */
struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr   idtp;

/* Assembly routine to load the IDT register */
extern void idt_flush(uint32_t);

/* 
 *  Set gate ‘n’ in the IDT to point at handler_addr.
 *  Flags: 0x8E = present, ring0, 32-bit interrupt gate.
 */
void set_idt_gate(uint8_t n, uint32_t handler_addr) {
    idt[n].base_lo  = (uint16_t)(handler_addr & 0xFFFF);
    idt[n].sel      = 0x08;                /* kernel code segment */
    idt[n].always0  = 0;
    idt[n].flags    = 0x8E;
    idt[n].base_hi  = (uint16_t)((handler_addr >> 16) & 0xFFFF);
}

/*
 *  Install the IDT we’ve been building.
 *  Does NOT clear out idt[]—we rely on the loader’s zero init.
 *  You must call set_idt_gate() for all of your handlers
 *  *before* invoking this.
 */
void idt_install(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    /* Flush the CPU’s IDTR to point at our table */
    idt_flush((uint32_t)&idtp);
}


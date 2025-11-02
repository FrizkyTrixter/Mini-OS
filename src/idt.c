/* File: src/idt.c */
#include "idt.h"
#include <stdint.h>

/* The actual IDT and its pointer */
struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr   idtp;

/* Assembly routine to load the IDT (defined in idt_flush.s) */
extern void idt_flush(uint32_t);

/* ────────────────────────────────────────────────────────────────
 * set_idt_gate()
 * Configures a single IDT entry.
 * Flags: 0x8E = Present, Ring 0, 32-bit interrupt gate
 * ──────────────────────────────────────────────────────────────── */
void set_idt_gate(uint8_t n, uint32_t handler_addr) {
    idt[n].base_lo  = (uint16_t)(handler_addr & 0xFFFF);
    idt[n].sel      = 0x08;      /* Kernel code segment selector */
    idt[n].always0  = 0;
    idt[n].flags    = 0x8E;      /* Present, Ring0, 32-bit interrupt gate */
    idt[n].base_hi  = (uint16_t)((handler_addr >> 16) & 0xFFFF);
}

/* ────────────────────────────────────────────────────────────────
 * idt_install()
 * Loads the new IDT pointer into the CPU’s IDTR register.
 * This function does not zero the IDT; it assumes it's pre-zeroed.
 * Call set_idt_gate() for each ISR/IRQ before calling this.
 * ──────────────────────────────────────────────────────────────── */
void idt_install(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    /* Load the IDT register (IDTR) */
    idt_flush((uint32_t)&idtp);
}

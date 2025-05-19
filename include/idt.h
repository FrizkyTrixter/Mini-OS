/* File: include/idt.h */
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* How many entries we’ll have in our IDT */
#define IDT_ENTRIES 256

/* An IDT entry (packed) */
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;        /* Kernel segment selector */
    uint8_t  always0;
    uint8_t  flags;      /* Flags (type, DPL, P) */
    uint16_t base_hi;
} __attribute__((packed));

/* The pointer structure used by lidt */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;       /* Address of first element in idt_entry array */
} __attribute__((packed));

/* Set an individual IDT gate. ‘n’ is the vector number (0–255). */
void set_idt_gate(uint8_t n, uint32_t handler_addr);

/* Loads the IDT via the lidt instruction */
void idt_install(void);

#endif /* IDT_H */


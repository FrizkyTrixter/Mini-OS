/* File: include/idt.h */
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Total number of IDT entries */
#define IDT_ENTRIES 256

/* ────────────────────────────────────────────────────────────────
 * IDT Entry Structure
 * Represents one interrupt gate in the IDT.
 * base_lo/base_hi : 32-bit address split into two halves
 * sel             : Kernel code segment selector
 * always0         : Reserved, always zero
 * flags           : Type and privilege flags
 * ──────────────────────────────────────────────────────────────── */
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

/* Structure loaded into the CPU IDTR register with lidt */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* ────────────────────────────────────────────────────────────────
 * Function Prototypes
 * ──────────────────────────────────────────────────────────────── */

/**
 * set_idt_gate - Set a single IDT entry
 * @n: interrupt vector (0–255)
 * @handler_addr: 32-bit address of the ISR/IRQ handler
 */
void set_idt_gate(uint8_t n, uint32_t handler_addr);

/**
 * idt_install - Load the IDT using lidt
 * This function must be called once all entries are configured.
 */
void idt_install(void);

#endif /* IDT_H */

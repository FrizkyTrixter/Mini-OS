/* File: src/exceptions.c */
#include <stdint.h>
#include "console.h"
#include "idt.h"
#include "exceptions.h"

/* The 32 exception ISR stubs provided by isr_exceptions.s */
extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

/* Names for pretty output */
static const char* exc_name[32] = {
    "Divide-by-zero (#DE)",            "Debug (#DB)",                   "NMI",
    "Breakpoint (#BP)",                "Overflow (#OF)",                "BOUND range (#BR)",
    "Invalid opcode (#UD)",            "Device not available (#NM)",    "Double fault (#DF)",
    "Coprocessor segment overrun",     "Invalid TSS (#TS)",             "Segment not present (#NP)",
    "Stack-segment fault (#SS)",       "General protection (#GP)",      "Page fault (#PF)",
    "Reserved",                        "x87 FP exception (#MF)",        "Alignment check (#AC)",
    "Machine check (#MC)",             "SIMD FP exception",             "Virtualization (#VE)",
    "Control-protection (#CP)",        "Reserved",                      "Reserved",
    "Reserved",                        "Reserved",                      "Reserved",
    "Reserved",                        "Reserved",                      "Reserved",
    "Reserved",                        "Reserved"
};

/* Small helpers */
static inline void print_hex32(uint32_t v) { print_hex(v); }

static void print_pf_err(uint32_t err) {
    print("  PF err: ");
    print((err & 1) ? "present " : "not-present ");
    print((err & 2) ? "write "   : "read ");
    print((err & 4) ? "user "    : "kernel ");
    if (err & 8)  print("rsvd ");
    if (err & 16) print("insn ");
    print("\n");
}

/* C-level handler that the assembly stubs call. */
void exception_handler_c(uint32_t vec, uint32_t errcode) {
    print("*** EXCEPTION: ");
    if (vec < 32) print(exc_name[vec]); else print("Unknown");
    print("  vec="); print_hex32(vec);
    print("  err="); print_hex32(errcode);
    print("\n");

    if (vec == 14) {                       /* #PF: show CR2 + err bits */
        uint32_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        print("  CR2 (fault addr) = 0x"); print_hex32(cr2); print("\n");
        print_pf_err(errcode);
    }

    /* Non-fatal/interactive: allow continuing from BP or OF if you want */
    if (vec == 3 /*#BP*/ || vec == 4 /*#OF*/) {
        return; /* iret back */
    }

    /* Everything else: stop to avoid re-fault loop */
    print("System halted.\n");
    for (;;) asm volatile("cli; hlt");
}

/* Wire the first 32 IDT entries to the ISR stubs. Call this before idt_install(). */
void exceptions_install(void) {
    set_idt_gate(0,  (uint32_t)isr0);   set_idt_gate(1,  (uint32_t)isr1);
    set_idt_gate(2,  (uint32_t)isr2);   set_idt_gate(3,  (uint32_t)isr3);
    set_idt_gate(4,  (uint32_t)isr4);   set_idt_gate(5,  (uint32_t)isr5);
    set_idt_gate(6,  (uint32_t)isr6);   set_idt_gate(7,  (uint32_t)isr7);
    set_idt_gate(8,  (uint32_t)isr8);   set_idt_gate(9,  (uint32_t)isr9);
    set_idt_gate(10, (uint32_t)isr10);  set_idt_gate(11, (uint32_t)isr11);
    set_idt_gate(12, (uint32_t)isr12);  set_idt_gate(13, (uint32_t)isr13);
    set_idt_gate(14, (uint32_t)isr14);  set_idt_gate(15, (uint32_t)isr15);
    set_idt_gate(16, (uint32_t)isr16);  set_idt_gate(17, (uint32_t)isr17);
    set_idt_gate(18, (uint32_t)isr18);  set_idt_gate(19, (uint32_t)isr19);
    set_idt_gate(20, (uint32_t)isr20);  set_idt_gate(21, (uint32_t)isr21);
    set_idt_gate(22, (uint32_t)isr22);  set_idt_gate(23, (uint32_t)isr23);
    set_idt_gate(24, (uint32_t)isr24);  set_idt_gate(25, (uint32_t)isr25);
    set_idt_gate(26, (uint32_t)isr26);  set_idt_gate(27, (uint32_t)isr27);
    set_idt_gate(28, (uint32_t)isr28);  set_idt_gate(29, (uint32_t)isr29);
    set_idt_gate(30, (uint32_t)isr30);  set_idt_gate(31, (uint32_t)isr31);
}

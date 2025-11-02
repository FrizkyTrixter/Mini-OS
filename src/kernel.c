/* File: src/kernel.c */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "paging.h"
#include "pmm.h"
#include "console.h"
#include "heap.h"
#include "timer.h"
#include "exceptions.h"

/* Some consoles don't declare this */
extern void print_dec(uint32_t);

/* Minimal strcmp (freestanding) */
static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

/* --- tiny helper: let users type '-' instead of '_' in commands --- */
static void normalize_cmd(char *s) {
    for (; *s; ++s) if (*s == '-') *s = '_';
}

/* Exception tests */
static void test_div0(void) { volatile uint32_t x=1, y=0; (void)(x/y); }
static void test_pagefault(void) { volatile uint32_t *p=(uint32_t*)0xDEADBEEF; *p=42; }
static void test_ud2(void) { asm volatile (".byte 0x0F, 0x0B"); }
static void test_int3(void) { asm volatile ("int3"); }

/* Primitive to draw a character at (row,col) in colour */
extern size_t get_cursor_pos(void);
extern void   print_char_at(char c, size_t row, size_t col, uint8_t colour);

/* IRQ service routine stubs */
extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

/* Keyboard shell/spinner state */
static bool   shift_pressed = false;
#define LINEBUF_SIZE 128
static char   linebuf[LINEBUF_SIZE];
static size_t linepos = 0;

static size_t prompt_row;
static const size_t prompt_prefix = 2;

/* ---- LIFO stack for up to 1024 allocated pages ---- */
#define PALLOC_STACK_SIZE 1024
static uint32_t palloc_stack[PALLOC_STACK_SIZE];
static int      palloc_stack_top = 0;
/* -------------------------------------------------- */

/* Canonical US PS/2 Set-1 keymaps (index == scancode) */
static const char keymap[128] = {
/*00*/  0,   27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
/*0F*/ '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',   0,
/*1E*/ 'a','s','d','f','g','h','j','k','l',';','\'','`',  0, '\\','z','x',
/*2E*/ 'c','v','b','n','m',',','.','/',  0,   '*',  0,  ' ',   0,   0,   0,  0,
/*3E*/  0,    0,  0,  0,   0,   0,   0,   0,  0,    0,   0,    0,   0,   0,  0,  0
};

static const char keymap_shift[128] = {
/*00*/  0,   27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
/*0F*/ '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',   0,
/*1E*/ 'A','S','D','F','G','H','J','K','L',':','"','~',  0,  '|','Z','X',
/*2E*/ 'C','V','B','N','M','<','>','?',  0,   '*',  0,  ' ',   0,   0,   0,  0,
/*3E*/  0,    0,  0,  0,   0,   0,   0,   0,  0,    0,   0,    0,   0,   0,  0,  0
};

/* Draw the prompt “> ” + spinner */
static void draw_prompt(void) {
    prompt_row = get_cursor_pos() / 80;
    print("> ");
    size_t spinner_col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_jiffies() % 4];
    print_char_at(spin, prompt_row, spinner_col, 0x0E);
}

/* Quick CR0/CR3 tests */
static void test_cr0_paging(void) {
    uint32_t cr0; asm volatile("mov %%cr0, %0" : "=r"(cr0));
    print(cr0 & (1u << 31) ? ">> CR0.PG = 1 (paging is ON)\n"
                           : ">> CR0.PG = 0 (paging is OFF)\n");
}
static void test_cr3(void) {
    uint32_t cr3; asm volatile("mov %%cr3, %0" : "=r"(cr3));
    print(">> CR3 = 0x"); print_hex(cr3); print("\n");
}

/* Execute a shell command */
static void shell_execute(char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        print("Available commands:\n");
        print("  help        - show this help\n");
        print("  clear       - clear the screen\n");
        print("  pminit      - initialize physical page allocator\n");
        print("  palloc      - allocate one 4KiB page\n");
        print("  pfree       - free the last allocated page\n");
        print("  ptest       - run a quick allocate/write/free test\n");
        print("  ticks       - show timer tick count\n");
        print("  sleep N     - sleep N ms (100 Hz PIT)\n");
        print("  hz          - check PIT frequency (~100Hz)\n");
        print("  test_div0   - trigger #DE (divide by zero)\n");
        print("  test_pf     - trigger #PF (page fault)\n");
        print("  test_ud     - trigger #UD (invalid opcode)\n");
        print("  test_bp     - trigger #BP (breakpoint)\n");
    }
    else if (strcmp(cmd, "clear") == 0) {
        clear_screen(); linepos = 0; draw_prompt(); return;
    }
    else if (strcmp(cmd, "pminit") == 0) {
        pmm_init(); print("Physical page allocator initialized.\n");
    }
    else if (strcmp(cmd, "palloc") == 0) {
        uint32_t page = pmm_alloc_page();
        if (!page) print("pmm_alloc_page() failed.\n");
        else {
            if (palloc_stack_top < PALLOC_STACK_SIZE)
                palloc_stack[palloc_stack_top++] = page;
            print("Allocated 4KiB page @ 0x"); print_hex(page); print("\n");
        }
    }
    else if (strcmp(cmd, "pfree") == 0) {
        if (palloc_stack_top == 0) print("No page to free.\n");
        else {
            uint32_t page = palloc_stack[--palloc_stack_top];
            pmm_free_page(page);
            print("Freed page @ 0x"); print_hex(page); print("\n");
        }
    }
    else if (strcmp(cmd, "ptest") == 0) {
        uint32_t page = pmm_alloc_page();
        if (!page) { print("ptest: alloc failed.\n"); }
        else {
            print("ptest: page @ 0x"); print_hex(page);
            print(". Writing test pattern...\n");
            volatile uint32_t *p = (uint32_t*)page;
            *p = 0xCAFEBABE;
            if (*p == 0xCAFEBABE) print("ptest: OK\n");
            else print("ptest: FAILED\n");
            pmm_free_page(page);
            print("ptest: freed 0x"); print_hex(page); print("\n");
        }
    }
    else if (strcmp(cmd, "ticks") == 0) {
        print_dec((uint32_t)timer_jiffies()); print("\n");
    }
    else if (cmd[0]=='s' && cmd[1]=='l' && cmd[2]=='e' && cmd[3]=='e' && cmd[4]=='p' && (cmd[5]==' ' || cmd[5]=='\t')) {
        uint32_t ms = 0;
        const char *arg = cmd + 6;
        while (*arg >= '0' && *arg <= '9') { ms = ms * 10 + (*arg - '0'); arg++; }
        print("Sleeping for "); print_dec(ms); print(" ms...\n");
        sleep_ms(ms);
        print("Woke up!\n");
    }
    else if (strcmp(cmd, "hz") == 0) {
        uint64_t start = timer_jiffies();
        sleep_ticks(100);
        uint64_t diff = timer_jiffies() - start;
        print("Waited "); print_dec((uint32_t)diff);
        print(" ticks (~"); print_dec((uint32_t)(diff*10)); print(" ms)\n");
    }
    else if (strcmp(cmd, "test_div0") == 0) {
        print("Triggering #DE...\n"); test_div0();
    }
    else if (strcmp(cmd, "test_pf") == 0) {
        print("Triggering #PF...\n"); test_pagefault();
    }
    else if (strcmp(cmd, "test_ud") == 0) {
        print("Triggering #UD...\n"); test_ud2();
    }
    else if (strcmp(cmd, "test_bp") == 0) {
        print("Triggering #BP...\n"); test_int3();
    }
    else {
        print("Unknown command: "); print(cmd); print("\n");
    }

    linepos = 0; draw_prompt();
}

/* Common IRQ handler */
void irq_handler_common(uint32_t irq, uint32_t err_code) {
    (void)err_code;

    /* erase old spinner */
    size_t col = prompt_prefix + linepos;
    print_char_at(' ', prompt_row, col, 0x0F);

    if (irq == 0) {
        timer_tick();
    }
    else if (irq == 1) {
        uint8_t sc = inb(0x60);
        if (sc == 0x2A || sc == 0x36) { shift_pressed = true; pic_send_eoi((uint8_t)irq); return; }
        if (sc == 0xAA || sc == 0xB6) { shift_pressed = false; pic_send_eoi((uint8_t)irq); return; }
        if (sc & 0x80) { pic_send_eoi((uint8_t)irq); return; } /* key release */

        if (sc == 0x0E) {  /* backspace */
            if (linepos > 0) {
                linepos--;
                print_char_at(' ', prompt_row, prompt_prefix + linepos, 0x0F);
            }
        }
        else if (sc == 0x1C) {  /* Enter */
            linebuf[linepos] = '\0';
            normalize_cmd(linebuf);      /* make '-' behave like '_' */
            shell_execute(linebuf);
            pic_send_eoi((uint8_t)irq);
            return;                      /* prompt redrawn inside shell */
        }
        else {
            char c = 0;
            if (sc < 128) c = shift_pressed ? keymap_shift[sc] : keymap[sc];
            if (c) {
                if (linepos < LINEBUF_SIZE - 1) {
                    linebuf[linepos] = c;
                    print_char_at(c, prompt_row, prompt_prefix + linepos, 0x0F);
                    linepos++;
                }
            }
        }
    }

    /* draw new spinner based on jiffies */
    col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_jiffies() % 4];
    print_char_at(spin, prompt_row, col, 0x0E);

    pic_send_eoi((uint8_t)irq);
}

/* Kernel entry point */
void kmain(void) {
    gdt_init();
    init_paging();
    heap_init();

    clear_screen();
    test_cr0_paging();
    test_cr3();
    print("Hello world!\nWelcome to your tiny OS!\n");

    void *p = kmalloc(64, 8);
    if (p) {
        *(uint32_t*)p = 0xDEADBEEF;
        print("Heap test OK @ 0x"); print_hex((uint32_t)p);
        print("\n");
    }

    pic_remap(0x20, 0x28);
    outb(PIC1_DATA, 0xFC);  /* unmask IRQ0/1 */
    outb(PIC2_DATA, 0xFF);

    exceptions_install();   /* CPU exceptions 0–31 */

    /* IRQ gates 32–47 */
    set_idt_gate(32,(uint32_t)irq0);  set_idt_gate(33,(uint32_t)irq1);
    set_idt_gate(34,(uint32_t)irq2);  set_idt_gate(35,(uint32_t)irq3);
    set_idt_gate(36,(uint32_t)irq4);  set_idt_gate(37,(uint32_t)irq5);
    set_idt_gate(38,(uint32_t)irq6);  set_idt_gate(39,(uint32_t)irq7);
    set_idt_gate(40,(uint32_t)irq8);  set_idt_gate(41,(uint32_t)irq9);
    set_idt_gate(42,(uint32_t)irq10); set_idt_gate(43,(uint32_t)irq11);
    set_idt_gate(44,(uint32_t)irq12); set_idt_gate(45,(uint32_t)irq13);
    set_idt_gate(46,(uint32_t)irq14); set_idt_gate(47,(uint32_t)irq15);

    idt_install();
    timer_init(100);
    asm volatile("sti");

    linepos = 0; draw_prompt();
    for (;;) asm volatile("hlt");
}

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

/* Minimal strcmp (freestanding) */
static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (uint8_t)*a - (uint8_t)*b;
}

/* Primitive to draw a character at (row,col) in colour */
extern size_t get_cursor_pos(void);
extern void   print_char_at(char c, size_t row, size_t col, uint8_t colour);

/* IRQ service routine stubs */
extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

/* Shell/spinner state */
static uint64_t timer_ticks   = 0;
static bool     shift_pressed = false;

#define LINEBUF_SIZE 128
static char linebuf[LINEBUF_SIZE];
static size_t linepos = 0;

static size_t prompt_row;
static const size_t prompt_prefix = 2;

/* ---- LIFO stack for up to 1024 allocated pages ---- */
#define PALLOC_STACK_SIZE 1024
static uint32_t palloc_stack[PALLOC_STACK_SIZE];
static int      palloc_stack_top = 0;
/* -------------------------------------------------- */

/* PS/2 scancode → ASCII map (partial) */
static const char scancode_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x10] = 'q', [0x11] = 'w',
    [0x12] = 'e', [0x13] = 'r', [0x14] = 't', [0x15] = 'y',
    [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n', [0x1E] = 'a',
    [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x39] = ' '
};

/* Draw the prompt “> ” + spinner */
static void draw_prompt(void) {
    prompt_row = get_cursor_pos() / 80;
    print("> ");
    size_t spinner_col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_ticks % 4];
    print_char_at(spin, prompt_row, spinner_col, 0x0E);
}

/* Quick CR0/CR3 tests */
static void test_cr0_paging(void) {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    print(cr0 & (1u << 31)
        ? ">> CR0.PG = 1 (paging is ON)\n"
        : ">> CR0.PG = 0 (paging is OFF)\n");
}

static void test_cr3(void) {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    print(">> CR3 = 0x");
    print_hex(cr3);
    print("\n");
}

/* Execute a shell command */
static void shell_execute(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        print("Available commands:\n");
        print("  help   - show this help\n");
        print("  clear  - clear the screen\n");
        print("  pminit - initialize the physical page allocator\n");
        print("  palloc - allocate one 4KiB page\n");
        print("  pfree  - free the last allocated page\n");
        print("  ptest  - run a quick allocate/write/free test\n");
    }
    else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
        linepos = 0;
        draw_prompt();
        return;
    }
    else if (strcmp(cmd, "pminit") == 0) {
        pmm_init();
        print("Physical page allocator initialized.\n");
    }
    else if (strcmp(cmd, "palloc") == 0) {
        uint32_t page = pmm_alloc_page();
        if (page == 0) {
            print("pmm_alloc_page() failed: no free frame.\n");
        } else {
            if (palloc_stack_top < PALLOC_STACK_SIZE)
                palloc_stack[palloc_stack_top++] = page;
            print("Allocated one 4KiB page at physical=0x");
            print_hex(page);
            print("\n");
        }
    }
    else if (strcmp(cmd, "pfree") == 0) {
        if (palloc_stack_top == 0) {
            print("No page to free. Use palloc first.\n");
        } else {
            uint32_t page = palloc_stack[--palloc_stack_top];
            pmm_free_page(page);
            print("Freed page at physical=0x");
            print_hex(page);
            print("\n");
        }
    }
    else if (strcmp(cmd, "ptest") == 0) {
        uint32_t page = pmm_alloc_page();
        if (page == 0) {
            print("ptest: pmm_alloc_page() failed.\n");
        } else {
            print("ptest: Allocated page at 0x");
            print_hex(page);
            print(". Writing test pattern...\n");
            volatile uint32_t *p = (uint32_t *)page;
            *p = 0xCAFEBABE;
            if (*p == 0xCAFEBABE)
                print("ptest: Write/read OK. Freeing page...\n");
            else
                print("ptest: Write/read FAILED.\n");
            pmm_free_page(page);
            print("ptest: Page 0x");
            print_hex(page);
            print(" freed.\n");
        }
    }
    else {
        print("Unknown command: ");
        print(cmd);
        print("\n");
    }

    linepos = 0;
    draw_prompt();
}

/* Common IRQ handler */
void irq_handler_common(uint32_t irq, uint32_t err_code) {
    (void)err_code;
    pic_send_eoi((uint8_t)irq);

    /* Erase old spinner */
    size_t col = prompt_prefix + linepos;
    print_char_at(' ', prompt_row, col, 0x0F);

    if (irq == 0) {
        timer_ticks++;
    }
    else if (irq == 1) {
        uint8_t sc = inb(0x60);
        if (sc == 0x2A || sc == 0x36) { shift_pressed = true; return; }
        if (sc == 0xAA || sc == 0xB6) { shift_pressed = false; return; }
        if (sc & 0x80) return;

        if (sc == 0x0E) {
            if (linepos > 0) {
                linepos--;
                print_char_at(' ', prompt_row, prompt_prefix + linepos, 0x0F);
            }
        }
        else if (sc == 0x1C) {
            linebuf[linepos] = '\0';
            shell_execute(linebuf);
            return;
        }
        else {
            char c = (sc < 128 ? scancode_map[sc] : 0);
            if (c) {
                if (shift_pressed && c >= 'a' && c <= 'z')
                    c = c - 'a' + 'A';
                if (linepos < LINEBUF_SIZE - 1) {
                    linebuf[linepos] = c;
                    print_char_at(c, prompt_row, prompt_prefix + linepos, 0x0F);
                    linepos++;
                }
            }
        }
    }

    /* Draw new spinner */
    col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_ticks % 4];
    print_char_at(spin, prompt_row, col, 0x0E);
}

/* Kernel entry point */
void kmain(void) {
    gdt_init();
    init_paging();

    /* Initialize heap using your existing no-arg API */
    heap_init();

    clear_screen();
    test_cr0_paging();
    test_cr3();
    print("Hello world!\n");
    print("Welcome to your tiny OS!\n");
    print("=== HEAP TEST ===\n");

    /* Simple heap test that matches your kmalloc(size, align) signature */
    void *p = kmalloc(64, 8);
    if (p) {
        *(uint32_t*)p = 0xDEADBEEF;
        print("Allocated @ 0x");
        print_hex((uint32_t)p);
        print(" -> wrote 0x");
        print_hex(*(uint32_t*)p);
        print("\n");
    } else {
        print("kmalloc failed!\n");
    }

    print("=== END TEST ===\n");

    /* Initialize PIC and IDT */
    pic_remap(0x20, 0x28);
    outb(PIC1_DATA, 0xFC);  /* unmask timer+keyboard */
    outb(PIC2_DATA, 0xFF);

    set_idt_gate(32, (uint32_t)irq0);
    set_idt_gate(33, (uint32_t)irq1);
    set_idt_gate(34, (uint32_t)irq2);
    set_idt_gate(35, (uint32_t)irq3);
    set_idt_gate(36, (uint32_t)irq4);
    set_idt_gate(37, (uint32_t)irq5);
    set_idt_gate(38, (uint32_t)irq6);
    set_idt_gate(39, (uint32_t)irq7);
    set_idt_gate(40, (uint32_t)irq8);
    set_idt_gate(41, (uint32_t)irq9);
    set_idt_gate(42, (uint32_t)irq10);
    set_idt_gate(43, (uint32_t)irq11);
    set_idt_gate(44, (uint32_t)irq12);
    set_idt_gate(45, (uint32_t)irq13);
    set_idt_gate(46, (uint32_t)irq14);
    set_idt_gate(47, (uint32_t)irq15);

    idt_install();
    asm volatile("sti");

    linepos = 0;
    draw_prompt();

    for (;;) {
        asm volatile("hlt");
    }
}

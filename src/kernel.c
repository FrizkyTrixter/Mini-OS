/* File: src/kernel.c */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "paging.h"    /* ← NEW: paging API */
#include "pmm.h"
#include "console.h"
#include "heap.h"/* ← Our new scrollable console driver */

/* Minimal strcmp (freestanding) */
static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

/* 
 * In order to draw the spinner or edit individual characters on‐screen, we still
 * need a “draw at (row, col)” primitive. We assume that console.c (and console.h)
 * also export a function with this signature:
 *
 *   void print_char_at(char c, size_t row, size_t col, uint8_t colour);
 *
 * If your console.h does not yet provide print_char_at(), you can add it:
 *
 *   // console.h
 *   void print(const char *s);
 *   void print_char(char c);
 *   void clear_screen(void);
 *   void print_hex(uint32_t value);
 *   size_t get_cursor_pos(void);
 *   void print_char_at(char c, size_t row, size_t col, uint8_t colour);
 *
 * And in console.c, implement print_char_at by directly writing to VGA memory,
 * and get_cursor_pos() by returning the internal cursor_pos. For this kernel.c
 * to compile as‐is, we declare them `extern` here:
 */
extern size_t get_cursor_pos(void);
extern void        print_char_at(char c, size_t row, size_t col, uint8_t colour);

/* Shared IRQ stubs (defined in entry.S or a similar file) */
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

/* Spinner + shell state */
static uint64_t timer_ticks   = 0;
static bool     shift_pressed = false;

#define LINEBUF_SIZE 128
static char linebuf[LINEBUF_SIZE];
static size_t linepos = 0;

/* Prompt sits on this row; we track it so that the spinner can be erased/redrawn */
static size_t prompt_row;
/* The prompt prefix is "> " (two characters) */
static const size_t prompt_prefix = 2;

/* Track the last physical page allocated so that “pfree” knows what to free */
static uint32_t last_alloc_page = 0;

/*
 * PS/2 scancode → ASCII (partial). We only handle keys we expect in our simple shell.
 */
static const char scancode_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n', [0x1E] = 'a', [0x1F] = 's',
    [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j',
    [0x25] = 'k', [0x26] = 'l', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
    [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x39] = ' '
};

/*
 * draw_prompt()
 *
 *   1) Queries the current cursor position (via get_cursor_pos()) to determine
 *      which VGA row we are on. We store that in prompt_row.
 *   2) Prints the literal string "> " so that our next user input appears after it.
 *   3) Computes the spinner’s column => prompt_prefix + linepos.
 *   4) Draws the spinner character at (prompt_row, spinner_col) in colour 0x0E.
 */
static void draw_prompt(void) {
    prompt_row = get_cursor_pos() / 80;   // each row is 80 characters wide
    print("> ");
    size_t spinner_col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_ticks % 4];
    print_char_at(spin, prompt_row, spinner_col, 0x0E);
}

/*
 * test_cr0_paging()
 *
 *   Simple check of the PG bit (bit 31) in CR0. If set, we’re in paging mode.
 */
static void test_cr0_paging(void) {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    if (cr0 & (1u << 31)) {
        print(">> CR0.PG = 1 (paging is ON)\n");
    } else {
        print(">> CR0.PG = 0 (paging is OFF)\n");
    }
}

/*
 * test_cr3()
 *
 *   Reads CR3 (the page-directory base register) and prints it in hex.
 *   We rely on console.c exporting a print_hex(uint32_t) function.
 */
static void test_cr3(void) {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    print(">> CR3 = 0x");
    print_hex(cr3);
    print("\n");
}

/*
 * shell_execute(cmd)
 *
 *   Called when the user presses Enter (scancode 0x1C). We assume `cmd` is
 *   a null-terminated string in linebuf[]. We:
 *
 *   1) Compare `cmd` against known commands via strcmp().
 *   2) On each branch, call the appropriate routines (clear_screen, pmm_init, etc.)
 *   3) If unknown, call print("Unknown command: "); print(cmd); print("\n");
 *   4) Finally, reset linepos = 0 and call draw_prompt() to show “> ” again.
 *
 *   Note: Because our console driver’s print() already forces a newline if
 *   we are mid‐line, we do NOT need to manually append “\n” before each message.
 */
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
        // After clear_screen, cursor is at row0,col0 => just redraw prompt
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
            last_alloc_page = page;
            print("Allocated one 4KiB page at physical=0x");
            print_hex(page);
            print("\n");
        }
    }
    else if (strcmp(cmd, "pfree") == 0) {
        if (last_alloc_page == 0) {
            print("No page to free. Use palloc first.\n");
        } else {
            pmm_free_page(last_alloc_page);
            print("Freed page at physical=0x");
            print_hex(last_alloc_page);
            print("\n");
            last_alloc_page = 0;
        }
    }
    else if (strcmp(cmd, "ptest") == 0) {
        /* Allocate a page, write a test pattern, verify, then free it */
        uint32_t page = pmm_alloc_page();
        if (page == 0) {
            print("ptest: pmm_alloc_page() failed.\n");
        } else {
            print("ptest: Allocated page at 0x");
            print_hex(page);
            print(". Writing test pattern...\n");

            /* Since paging is identity-mapped, physical == virtual */
            volatile uint32_t *p = (uint32_t *)page;
            *p = 0xCAFEBABE;
            if (*p == 0xCAFEBABE) {
                print("ptest: Write/read OK. Freeing page...\n");
            } else {
                print("ptest: Write/read FAILED.\n");
            }

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

    // Reset the line position and redraw the prompt on the next line
    linepos = 0;
    draw_prompt();
}

/*
 * irq_handler_common(irq, err_code)
 *
 *   1) Acknowledge (EOI) to the PIC.
 *   2) Erase the old spinner by drawing a space at (prompt_row, prompt_prefix+linepos).
 *   3) If irq==0, increment timer_ticks.
 *   4) If irq==1 (keyboard), decode the scancode:
 *       • Shift press/release toggles shift_pressed.
 *       • Key-release scancodes (0x80 bit set) are ignored (except shift).
 *       • Backspace (0x0E) deletes one char from linebuf (if any), erasing it on-screen.
 *       • Enter (0x1C) null-terminates linebuf, calls shell_execute(linebuf), and returns.
 *       • Otherwise, translate scancode→ASCII via scancode_map[], uppercase if shift,
 *         store in linebuf[linepos], draw on-screen via print_char_at(), increment linepos.
 *   5) Finally, draw the new spinner character at (prompt_row, prompt_prefix+linepos).
 */
void irq_handler_common(uint32_t irq, uint32_t err_code) {
    (void)err_code;
    pic_send_eoi((uint8_t)irq);

    /* Erase old spinner */
    size_t spinner_col = prompt_prefix + linepos;
    print_char_at(' ', prompt_row, spinner_col, 0x0F);

    if (irq == 0) {
        timer_ticks++;
    }
    else if (irq == 1) {
        uint8_t sc = inb(0x60);
        // Shift press (0x2A or 0x36) or release (0xAA or 0xB6)
        if (sc == 0x2A || sc == 0x36) { shift_pressed = true; return; }
        if (sc == 0xAA || sc == 0xB6) { shift_pressed = false; return; }
        if (sc & 0x80) return;  // ignore all “key up” scancodes except shift

        if (sc == 0x0E) {
            /* Backspace: if we have at least one character, erase it */
            if (linepos > 0) {
                linepos--;
                print_char_at(' ', prompt_row, prompt_prefix + linepos, 0x0F);
            }
        }
        else if (sc == 0x1C) {
            /* Enter: terminate the current linebuffer and run the command */
            linebuf[linepos] = '\0';
            shell_execute(linebuf);
            return;
        }
        else {
            /* Normal key: map via scancode_map[], apply shift if needed */
            char c = (sc < 128 ? scancode_map[sc] : 0);
            if (c) {
                if (shift_pressed && c >= 'a' && c <= 'z') {
                    c = c - 'a' + 'A';
                }
                if (linepos < LINEBUF_SIZE - 1) {
                    linebuf[linepos] = c;
                    print_char_at(c, prompt_row, prompt_prefix + linepos, 0x0F);
                    linepos++;
                }
            }
        }
    }

    /* Draw new spinner at updated position */
    spinner_col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_ticks % 4];
    print_char_at(spin, prompt_row, spinner_col, 0x0E);
}

/*
 * kmain()
 *
 *   1) Initialize GDT
 *   2) Enable paging (identity-mapping 0–4MiB) via init_paging()
 *   3) Clear the screen, test CR0.PG, test CR3, print a welcome message
 *   4) Remap the PIC to 0x20/0x28, mask/unmask IRQ lines
 *   5) Install IDT gates for IRQs 0–15 (vectors 32–47), then sti()
 *   6) Draw the first prompt and spin forever, halting on each loop
 */
void kmain(void) {
    gdt_init();

    init_paging();
    heap_init();/* ← NEW: enable 4KiB paging (identity-map 0–4MiB) */
    clear_screen();
    test_cr0_paging();
    test_cr3();
    print("Hello world!\n");
    print("Welcome to your tiny OS!\n");
    print("=== HEAP TEST ===\n");
    heap_dump_stats();

    // carve out 64 bytes
    void *p = kmalloc(64, 8);
    if (p) {
        *(uint32_t*)p = 0xDEADBEEF;
        print("Allocated @ 0x"); print_hex((uint32_t)p);
        print(" -> wrote 0x");    print_hex(*(uint32_t*)p);
        print("\n");
    }

    heap_dump_stats();
    print("=== END TEST ===\n");

    /* Remap PIC: IRQ0–IRQ7 → 0x20–0x27, IRQ8–IRQ15 → 0x28–0x2F */
    pic_remap(0x20, 0x28);
    outb(PIC1_DATA, 0xFC);   // Mask all except IRQ0 (timer) and IRQ1 (keyboard)
    outb(PIC2_DATA, 0xFF);   // Mask all on slave PIC

    /* Install IRQ handlers (vectors 32–47) */
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
    asm volatile("sti");    /* Enable interrupts */

    /* Draw the very first prompt (“> ” + spinner) */
    linepos = 0;
    draw_prompt();

    for (;;) {
        asm volatile("hlt");
    }
}

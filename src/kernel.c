/* File: src/kernel.c */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "paging.h"    /* ← NEW: paging API */

 /* Minimal strcmp (freestanding) */
static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

/* VGA text-mode buffer */
static uint16_t* const VGA       = (uint16_t*)0xB8000;
static const size_t    VGA_WIDTH = 80;
static const size_t    VGA_HEIGHT= 25;

/* “Cursor” for print() */
static size_t console_cursor = 0;

/* Build a VGA entry: character + colour */
static inline uint16_t vga_entry(char c, uint8_t colour) {
    return (uint16_t)c | (uint16_t)colour << 8;
}

/* Clear the screen */
static void clear_screen(void) {
    uint16_t blank = vga_entry(' ', 0x0F);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        VGA[i] = blank;
    }
    console_cursor = 0;
}

/* Print a string, handling '\n' and '\r' */
static void print(const char *s) {
    while (*s) {
        char c = *s++;
        if (c == '\n') {
            console_cursor = (console_cursor / VGA_WIDTH + 1) * VGA_WIDTH;
        } else if (c == '\r') {
            console_cursor = (console_cursor / VGA_WIDTH) * VGA_WIDTH;
        } else {
            VGA[console_cursor++] = vga_entry(c, 0x0F);
        }
        if (console_cursor >= VGA_WIDTH * VGA_HEIGHT) {
            console_cursor = 0;
        }
    }
}

/* Draw one character at (row,col) */
static void draw_char(char c, size_t row, size_t col, uint8_t colour) {
    if (row < VGA_HEIGHT && col < VGA_WIDTH) {
        VGA[row * VGA_WIDTH + col] = vga_entry(c, colour);
    }
}

/* IRQ stubs */
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

/* Prompt sits on this row */
static size_t prompt_row;
/* Prompt prefix "> " is two chars */
static const size_t prompt_prefix = 2;

/* PS/2 scancode → ASCII (partial) */
static const char scancode_map[128] = {
    [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',
    [0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',
    [0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',
    [0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
    [0x1A]='[',[0x1B]=']',[0x1C]='\n',[0x1E]='a',[0x1F]='s',
    [0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',
    [0x25]='k',[0x26]='l',[0x2C]='z',[0x2D]='x',[0x2E]='c',
    [0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',
    [0x34]='.',[0x35]='/',[0x39]=' '
};

/* Draw the prompt ("> ") and initial spinner position */
static void draw_prompt(void) {
    prompt_row = console_cursor / VGA_WIDTH;
    print("> ");
    size_t spinner_col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_ticks % 4];
    draw_char(spin, prompt_row, spinner_col, 0x0E);
}
static void test_cr0_paging(void) {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    if (cr0 & (1u<<31))
        print(">> CR0.PG = 1 (paging is ON)\n");
    else
        print(">> CR0.PG = 0 (paging is OFF!)\n");
}
static void test_cr3(void) {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    print(">> CR3 = 0x"); 
    // you’ll need a small hex-printer; e.g.:
    for (int i = 28; i >= 0; i -= 4) {
        char digit = "0123456789ABCDEF"[(cr3 >> i) & 0xF];
        // write digit with your existing print()—e.g. convert to a one-char string
        char s[2] = { digit, 0 };
        print(s);
    }
    print("\n");
}
/* Execute command line */
static void shell_execute(const char *cmd) {
    print("\n");
    if (strcmp(cmd, "help") == 0) {
        print("Available commands:\n");
        print("  help  - show this help\n");
        print("  clear - clear the screen\n");
    } else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } else {
        print("Unknown command: ");
        print(cmd);
        print("\n");
    }
    console_cursor = (console_cursor / VGA_WIDTH + 1) * VGA_WIDTH;
    linepos = 0;
    draw_prompt();
}

/* Shared IRQ handler */
void irq_handler_common(uint32_t irq, uint32_t err_code) {
    (void)err_code;
    pic_send_eoi((uint8_t)irq);

    /* Erase old spinner */
    size_t spinner_col = prompt_prefix + linepos;
    draw_char(' ', prompt_row, spinner_col, 0x0F);

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
                draw_char(' ', prompt_row, prompt_prefix + linepos, 0x0F);
            }
        }
        else if (sc == 0x1C) {
            linebuf[linepos] = '\0';
            shell_execute(linebuf);
            return;
        }
        else {
            char c = sc < 128 ? scancode_map[sc] : 0;
            if (c) {
                if (shift_pressed && c >= 'a' && c <= 'z')
                    c = c - 'a' + 'A';
                if (linepos < LINEBUF_SIZE-1) {
                    linebuf[linepos] = c;
                    draw_char(c, prompt_row, prompt_prefix + linepos, 0x0F);
                    linepos++;
                }
            }
        }
    }

    /* Draw spinner at new position */
    spinner_col = prompt_prefix + linepos;
    char spin = "|/-\\"[timer_ticks % 4];
    draw_char(spin, prompt_row, spinner_col, 0x0E);
}

/* Kernel entry */
void kmain(void) {
    gdt_init();

    init_paging();    /* ← NEW: enable 4 KiB paging (identity-map 0–4 MiB) */
    clear_screen();
    test_cr0_paging();
    test_cr3();
    print("Hello world!\n");
    print("Welcome to your tiny OS!\n");

    pic_remap(0x20, 0x28);
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);

    /* Install IRQ handlers 32–47 */
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

    /* Draw the first prompt with spinner */
    draw_prompt();

    for (;;) {
        asm volatile("hlt");
    }
}

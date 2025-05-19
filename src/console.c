#include <stdint.h>
#include <stddef.h>
#include "console.h"

static uint16_t* const VGA = (uint16_t*)0xB8000;
static const size_t VGA_WIDTH  = 80;
static const size_t VGA_HEIGHT = 25;

/* Build a VGA entry from char + colour */
static inline uint16_t vga_entry(char c, uint8_t colour) {
    return (uint16_t)c | (uint16_t)colour << 8;
}

/* Cursor index into the VGA buffer */
static size_t cursor_pos = 0;

void clear_screen(void) {
    uint16_t blank = vga_entry(' ', 0x0F);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        VGA[i] = blank;
    }
    cursor_pos = 0;
}

void print(const char* str) {
    while (*str) {
        char c = *str++;
        if (c == '\n') {
            cursor_pos = (cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
        } else if (c == '\r') {
            cursor_pos = (cursor_pos / VGA_WIDTH) * VGA_WIDTH;
        } else {
            VGA[cursor_pos++] = vga_entry(c, 0x0F);
        }
        if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
            cursor_pos = 0;
        }
    }
}

void print_char(char c) {
    if (c == '\n') {
        cursor_pos = (cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
    } else if (c == '\r') {
        cursor_pos = (cursor_pos / VGA_WIDTH) * VGA_WIDTH;
    } else {
        VGA[cursor_pos++] = vga_entry(c, 0x0F);
    }
    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        cursor_pos = 0;
    }
}


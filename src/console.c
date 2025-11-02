// ─────────────────────────────────────────────────────────────────────────────
// src/console.c
//
//   A scrollable VGA console driver for 80×25 text mode. Whenever you write
//   past row 24, the entire screen scrolls up one line and the cursor lands
//   at row 24, column 0.  Also provides helpers to draw an individual character
//   at (row,col), print eight hex digits, print decimal, and query cursor_pos.
// ─────────────────────────────────────────────────────────────────────────────

#include <stdint.h>
#include <stddef.h>
#include "console.h"

static uint16_t* const VGA        = (uint16_t*)0xB8000;
static const size_t   VGA_WIDTH   = 80;
static const size_t   VGA_HEIGHT  = 25;

/*
 * Build a single 16-bit VGA cell from (char c, uint8_t colour).
 * low byte = ASCII, high byte = palette attribute.
 */
static inline uint16_t vga_entry(char c, uint8_t colour) {
    return (uint16_t)c | ((uint16_t)colour << 8);
}

/*
 * The “logical” cursor as a linear index into the 80×25 buffer:
 *   row = cursor_pos / 80
 *   col = cursor_pos % 80
 */
static size_t cursor_pos = 0;

/*
 * scroll_if_needed()
 *
 *   If cursor_pos ≥ 80×25, we have written off the bottom row. Instead of
 *   wrapping back to (0,0), do:
 *
 *     1) Copy row 1→0, 2→1, …, 24→23
 *     2) Clear row 24 to spaces (attribute 0x0F)
 *     3) Set cursor_pos = 24×80  (i.e. first column of row 24)
 */
static void scroll_if_needed(void) {
    size_t max_cells = VGA_WIDTH * VGA_HEIGHT;
    if (cursor_pos < max_cells) {
        return;  // no scroll needed
    }

    // Copy each row r=1..24 into row (r−1)
    for (size_t row = 1; row < VGA_HEIGHT; row++) {
        for (size_t col = 0; col < VGA_WIDTH; col++) {
            VGA[(row - 1) * VGA_WIDTH + col] = VGA[row * VGA_WIDTH + col];
        }
    }

    // Clear row 24 with blanks (character ' ', colour 0x0F)
    const uint16_t blank = vga_entry(' ', 0x0F);
    for (size_t col = 0; col < VGA_WIDTH; col++) {
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;
    }

    // Move cursor_pos to the first column of row 24
    cursor_pos = (VGA_HEIGHT - 1) * VGA_WIDTH;
}

/*
 * clear_screen()
 *
 *   Fill the entire 80×25 buffer with space (colour 0x0F) and reset cursor_pos=0.
 */
void clear_screen(void) {
    const uint16_t blank = vga_entry(' ', 0x0F);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA[i] = blank;
    }
    cursor_pos = 0;
}

/*
 * print_char(char c)
 *
 *   Write a single character `c` at the current cursor position:
 *     - If c == '\n', move cursor_pos to the first column of the next row
 *     - If c == '\r', move cursor_pos to the first column of this row
 *     - Otherwise: VGA[cursor_pos] = (c,0x0F); cursor_pos++
 *   Then call scroll_if_needed() to guarantee no wrap-around.
 */
static void print_char(char c) {
    switch (c) {
        case '\n':
            // Move to first column of the next row:
            cursor_pos = (cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
            break;
        case '\r':
            // Move to first column of this row:
            cursor_pos = (cursor_pos / VGA_WIDTH) * VGA_WIDTH;
            break;
        default:
            VGA[cursor_pos++] = vga_entry(c, 0x0F);
            break;
    }
    scroll_if_needed();
}

/*
 * print(const char *s)
 *
 *   Walk through each character in the null-terminated string `s` and write it.
 *   This version forces a newline first if the cursor isn't at column 0 and
 *   the string does not begin with '\n'.
 */
void print(const char *s) {
    // If cursor is not at column 0 and the string does not start with '\n',
    // force a newline. (Keeps multi-word prints aligned.)
    if ((cursor_pos % VGA_WIDTH) != 0 && *s != '\n') {
        cursor_pos = (cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
        scroll_if_needed();
    }

    while (*s) {
        char c = *s++;
        if (c == '\n') {
            cursor_pos = (cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
        } else if (c == '\r') {
            cursor_pos = (cursor_pos / VGA_WIDTH) * VGA_WIDTH;
        } else {
            VGA[cursor_pos++] = vga_entry(c, 0x0F);
        }
        scroll_if_needed();
    }
}

/*
 * print_hex(uint32_t value)
 *
 *   Prints exactly eight hex digits (0–9A–F), e.g. 0x0000000F ⇒ "0000000F".
 */
void print_hex(uint32_t value) {
    char buf[9] = {0};
    static const char *hex_digits = "0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {
        buf[i] = hex_digits[value & 0xF];
        value >>= 4;
    }
    print(buf);
}

/*
 * print_dec(uint32_t value)
 *
 *   Prints an unsigned 32-bit value in decimal.
 */
void print_dec(uint32_t value) {
    char buf[11];  // max "4294967295" + NUL
    int i = 10;
    buf[i] = '\0';
    if (value == 0) {
        print("0");
        return;
    }
    while (value && i > 0) {
        uint32_t q = value / 10;
        uint32_t r = value - q * 10;
        buf[--i] = (char)('0' + r);
        value = q;
    }
    print(&buf[i]);
}

/*
 * get_cursor_pos(void)
 *
 *   Return the current linear cursor index. 0 ≤ cursor_pos < 80×25.
 */
size_t get_cursor_pos(void) {
    return cursor_pos;
}

/*
 * print_char_at(char c, size_t row, size_t col, uint8_t colour)
 *
 *   Write the single ASCII `c` at VGA[row][col] with palette `colour` (0x00–0xFF).
 *   Does NOT change the logical cursor_pos.
 */
void print_char_at(char c, size_t row, size_t col, uint8_t colour) {
    if (row < VGA_HEIGHT && col < VGA_WIDTH) {
        VGA[row * VGA_WIDTH + col] = vga_entry(c, colour);
    }
}

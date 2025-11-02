#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stddef.h>

/*
 * Clears the entire 25×80 text-mode screen and resets the internal cursor to (0,0).
 */
void clear_screen(void);

/*
 * Prints a null-terminated C string at the current cursor position.
 * - '\n' moves to the first column of the next row.
 * - '\r' moves to the first column of the same row.
 * - Otherwise, the character is placed at VGA[cursor_pos++] with attribute 0x0F.
 *   If you write past the bottom, the screen scrolls up one line automatically.
 */
void print(const char *s);

/*
 * Prints exactly eight hexadecimal digits corresponding to 'value'.
 * For example, print_hex(0x123) prints "00000123".
 */
void print_hex(uint32_t value);

/*
 * Prints an unsigned 32-bit integer in decimal (no sign).
 */
void print_dec(uint32_t value);

/*
 * Returns the current linear cursor position [0 .. 80*25−1].
 *   row = get_cursor_pos() / 80
 *   col = get_cursor_pos() % 80
 */
size_t get_cursor_pos(void);

/*
 * Prints a single ASCII character 'c' AT the specified (row,col) in VGA text mode,
 * using the palette attribute 'colour'. This does NOT move the “logical” cursor.
 *
 * row and col must satisfy 0 ≤ row < 25, 0 ≤ col < 80. Anything outside is ignored.
 */
void print_char_at(char c, size_t row, size_t col, uint8_t colour);

#endif  // CONSOLE_H

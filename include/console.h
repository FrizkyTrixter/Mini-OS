#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stddef.h>

/* Clears the VGA text buffer. */
void clear_screen(void);
/* Prints a NUL-terminated string, handling \n and \r. */
void print(const char* str);
/* Prints a single character (for echoing). */
void print_char(char c);

#endif // CONSOLE_H


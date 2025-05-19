#ifndef IO_H
#define IO_H

#include <stdint.h>

/* Read a byte from the given I/O port */
uint8_t inb(uint16_t port);
/* Write a byte to the given I/O port */
void    outb(uint16_t port, uint8_t data);

#endif // IO_H

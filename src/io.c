#include "io.h"

uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %[port], %[ret]"
                  : [ret]"=a"(ret)
                  : [port]"Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %[data], %[port]"
                  :
                  : [data]"a"(data), [port]"Nd"(port));
}


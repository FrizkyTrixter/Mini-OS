#ifndef PIC_H
#define PIC_H

#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

/* Remap master/slave PIC to vectors [offset1..offset1+7] & [offset2..offset2+7] */
void pic_remap(int offset1, int offset2);
/* Send end-of-interrupt signal for the given IRQ */
void pic_send_eoi(uint8_t irq);

#endif // PIC_H


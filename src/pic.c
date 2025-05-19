#include "pic.h"
#include "io.h"

void pic_remap(int offset1, int offset2) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (cascade mode) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    /* Set new vector offsets */
    outb(PIC1_DATA, offset1);
    outb(PIC2_DATA, offset2);

    /* Tell Master PIC there is a slave at IRQ2 (0x04) */
    outb(PIC1_DATA, 0x04);
    /* Tell Slave PIC its cascade identity (0x02) */
    outb(PIC2_DATA, 0x02);

    /* Configure for 8086/88 (MCS-80/85) mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        /* Notify slave */
        outb(PIC2_COMMAND, 0x20);
    }
    /* Notify master */
    outb(PIC1_COMMAND, 0x20);
}


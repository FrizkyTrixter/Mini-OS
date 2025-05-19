#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Nothing to init beyond PIC unmask; provided for symmetry */
void keyboard_init(void);
/* Called from irq_handler_common when IRQ1 fires */
void keyboard_handler(void);

#endif // KEYBOARD_H


; File: src/irq_stubs.s

    ; Declare the common C handler
    extern irq_handler_common

section .text
    global irq0
    global irq1
    global irq2
    global irq3
    global irq4
    global irq5
    global irq6
    global irq7
    global irq8
    global irq9
    global irq10
    global irq11
    global irq12
    global irq13
    global irq14
    global irq15

%macro IRQ_HANDLER 1
irq%1:
    cli                     ; disable interrupts
    push dword 0            ; dummy error code
    push dword %1           ; IRQ number
    call irq_handler_common ; call into C
    add esp, 8              ; clean up two dwords
    sti                     ; re-enable interrupts
    iret                    ; return from CPU
%endmacro

; Instantiate handlers for IRQ0–IRQ15
IRQ_HANDLER 0
IRQ_HANDLER 1
IRQ_HANDLER 2
IRQ_HANDLER 3
IRQ_HANDLER 4
IRQ_HANDLER 5
IRQ_HANDLER 6
IRQ_HANDLER 7
IRQ_HANDLER 8
IRQ_HANDLER 9
IRQ_HANDLER 10
IRQ_HANDLER 11
IRQ_HANDLER 12
IRQ_HANDLER 13
IRQ_HANDLER 14
IRQ_HANDLER 15


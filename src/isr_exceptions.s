; File: src/isr_exceptions.s
[BITS 32]
[GLOBAL isr0]
[GLOBAL isr1]
[GLOBAL isr2]
[GLOBAL isr3]
[GLOBAL isr4]
[GLOBAL isr5]
[GLOBAL isr6]
[GLOBAL isr7]
[GLOBAL isr8]
[GLOBAL isr9]
[GLOBAL isr10]
[GLOBAL isr11]
[GLOBAL isr12]
[GLOBAL isr13]
[GLOBAL isr14]
[GLOBAL isr15]
[GLOBAL isr16]
[GLOBAL isr17]
[GLOBAL isr18]
[GLOBAL isr19]
[GLOBAL isr20]
[GLOBAL isr21]
[GLOBAL isr22]
[GLOBAL isr23]
[GLOBAL isr24]
[GLOBAL isr25]
[GLOBAL isr26]
[GLOBAL isr27]
[GLOBAL isr28]
[GLOBAL isr29]
[GLOBAL isr30]
[GLOBAL isr31]

[EXTERN exception_handler_c]

; Macro for exceptions with no error code
%macro ISR_NOERR 1
isr%1:
    cli
    push dword 0        ; dummy error code
    push dword %1       ; vector number
    call exception_handler_c
    add esp, 8
    sti
    iretd
%endmacro

; Macro for exceptions with real error code
%macro ISR_ERR 1
isr%1:
    cli
    push dword %1       ; vector number
    call exception_handler_c
    add esp, 8          ; pop vec + err
    sti
    iretd
%endmacro

; Exceptions without error code:
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8          ; DF has errcode
ISR_NOERR 9
ISR_ERR 10         ; TSS
ISR_ERR 11         ; NP
ISR_ERR 12         ; SS
ISR_ERR 13         ; GP
ISR_ERR 14         ; PF
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17         ; AC
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

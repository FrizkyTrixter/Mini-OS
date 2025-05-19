;--------------------------------------------
; src/get_flush.asm
; Assembly helper to load IDT from memory
;--------------------------------------------
[BITS 32]
global idt_flush

; void idt_flush(uint32_t idt_ptr_addr);
idt_flush:
    push ebp
    mov  ebp, esp
    mov  eax, [ebp + 8]   ; load address-of-idt_ptr into EAX
    lidt [eax]            ; load the IDT
    pop  ebp
    ret


;----------------------------------------------------------
; Assembly helper to load GDT and reload segment registers
;----------------------------------------------------------
[BITS 32]
global gdt_flush

; void gdt_flush(uint32_t gdt_ptr_addr);
gdt_flush:
    cli                     ; disable interrupts

    ; load address-of-gdtr into EAX
    push ebp
    mov  ebp, esp
    mov  eax, [ebp + 8]
    pop  ebp

    ; load the new GDT
    lgdt [eax]

    ; reload data segment registers with selector 0x10
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; perform far jump to reload CS = 0x08
    push dword 0x08        ; code-segment selector
    push dword flush_label ; offset
    retf                   ; far return

flush_label:
    sti                     ; re-enable interrupts
    ret


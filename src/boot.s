; File: src/boot.s — Multiboot header + jump to C entry (kmain)

[BITS 32]
global _start
extern kmain

; ─── Multiboot Header (must be at top!) ────────────────────────
    dd 0x1BADB002        ; magic
    dd 0x00000003        ; flags: bit0=1 (align modules), bit1=1 (mem info)
    dd -(0x1BADB002 + 0x00000003)  ; checksum

section .text
_start:
    cli                  ; disable interrupts
    call kmain           ; jump into your C kernel
.halt:
    hlt                  ; wait for interrupt
    jmp .halt            ; loop


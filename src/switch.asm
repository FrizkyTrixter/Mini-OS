; File: src/switch.asm

global context_switch

context_switch:
    ; void context_switch(uint32_t *old_esp, uint32_t new_esp)
    ;
    ; [esp + 4] = old_esp pointer
    ; [esp + 8] = new_esp value

    pusha

    mov eax, [esp + 36]     ; old_esp pointer after pusha
    mov [eax], esp          ; save current ESP

    mov eax, [esp + 40]     ; new ESP
    mov esp, eax            ; switch to new stack

    popa
    ret
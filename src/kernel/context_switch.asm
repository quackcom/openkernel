; openkernel - Context Switching Assembly
; Handles saving and restoring CPU state between processes

[BITS 32]

[GLOBAL context_switch]
[EXTERN current_process]

; Context switch function
; void context_switch(uint32_t prev_esp, uint32_t next_esp)
context_switch:
    ; Save current context
    push ebp
    push ebx
    push esi
    push edi

    ; Get parameters
    mov eax, [esp + 20]    ; prev_esp
    mov ecx, [esp + 24]    ; next_esp

    ; Save current ESP to previous process's PCB
    mov [eax], esp

    ; Switch stacks
    mov esp, ecx

    ; Restore new context
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; Return to new process
    ret
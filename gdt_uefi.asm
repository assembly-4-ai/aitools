; gdt_uefi.asm: GDT Setup for post-UEFI 64-bit
; Depends on: boot_defs_temp.inc

BITS 64
%include "boot_defs_temp.inc" ; Needs CODE64_SEL, DATA64_SEL

global setup_final_gdt64
global load_gdt_and_segments64
global gdt64_pointer_var    ; Variable holding the GDTR value

section .data align=8
gdt64_pointer_var:
    dw 0 ; Limit (set at runtime)
    dq 0 ; Base (set at runtime)

section .text
;--------------------------------------------------------------------------
; setup_final_gdt64: Writes GDT descriptors to allocated memory.
; Input:
;   RDI = Physical address of allocated memory for GDT
; Output: None, GDT written at [RDI], gdt64_pointer_var updated
; Destroys: RAX
;--------------------------------------------------------------------------
setup_final_gdt64:
    mov rax, rdi ; RAX = GDT base physical address

    ; Null descriptor (index 0)
    mov qword [rax], 0

    ; Code64: Selector CODE64_SEL (typically 0x08 or 0x10 based on your choice)
    ; We use the defined CODE64_SEL from boot_defs_temp.inc (e.g., 0x18)
    ; Base=0, Limit=0xFFFFF (4G segments), Type=Code (Execute/Read),
    ; S=1 (Code/Data), DPL=0, P=1, AVL=0, L=1 (64-bit), D/B=0, G=1 (4KB granularity)
    mov qword [rax + CODE64_SEL], 0x00AF9A0000000000 ; Corrected in previous response

    ; Data64: Selector DATA64_SEL (typically 0x10 or 0x18)
    ; We use the defined DATA64_SEL from boot_defs_temp.inc (e.g., 0x20)
    ; Base=0, Limit=0xFFFFF (4G segments), Type=Data (Read/Write),
    ; S=1 (Code/Data), DPL=0, P=1, AVL=0, L=0, D/B=1 (32-bit stack, ignored in 64-bit for data), G=1
    mov qword [rax + DATA64_SEL], 0x00CF920000000000 ; Corrected in previous response

    ; Update GDTR variable
    ; Limit = (Number of entries * size_per_entry) - 1
    ; If last entry is DATA64_SEL, then size is DATA64_SEL + 8
    mov word [gdt64_pointer_var], (DATA64_SEL + 8 - 1) ; Limit = size - 1
    mov [gdt64_pointer_var + 2], rax ; Base address

    ret

;--------------------------------------------------------------------------
; load_gdt_and_segments64: Loads the GDT and refreshes segment registers
; Assumes gdt64_pointer_var is correctly populated.
; Call this AFTER ExitBootServices and after setting up the new GDT.
;--------------------------------------------------------------------------
load_gdt_and_segments64:
    lgdt [gdt64_pointer_var]

    ; Reload CS via far jump (using RETF equivalent with IRETQ)
    ; To reload CS, we need to perform an inter-segment jump or return.
    ; IRETQ is suitable here as it loads CS, RIP, RFLAGS, SS, RSP.
    ; We push values onto the stack to simulate a far call / interrupt return.

    push DATA64_SEL ; Target SS selector (must be a valid data segment)
    mov rax, rsp    ; Get current RSP
    ; We need to push a target RSP value that will be correct AFTER IRETQ pops SS and RSP.
    ; Since we're staying on the same stack, this RSP should be the current one
    ; or a new one if we were changing stacks. For just reloading segments, current is fine.
    ; However, IRETQ expects 5 qwords on stack. SS, RSP, RFLAGS, CS, RIP.
    ; The RSP we push will be the stack pointer *in the new segment context*.
    ; For a flat model, this RSP value can be the current RSP before these pushes.
    ; Let's ensure we have enough space on current stack and point to it.
    ; It's often simpler to just ensure the stack pointer is valid and SS is valid.
    add rax, 8*5    ; Calculate what RSP *would be* if these 5 qwords were popped.
                    ; This is a bit tricky. A simpler approach for just CS:
                    ; jmp CODE64_SEL:.reload_cs_target
                    ; This is not ideal as it doesn't fully serialize.
                    ; IRETQ provides full serialization.

    push rax        ; Target RSP for the new context (after IRETQ restores it)
    pushfq          ; Push RFLAGS
    push CODE64_SEL ; Push target CS selector
    lea rax, [rel .reload_cs_target_label] ; Push target RIP
    push rax
    iretq           ; Load CS, RIP, RFLAGS, RSP, SS

.reload_cs_target_label:
    ; Now running with new CS
    ; Reload data segments
    mov ax, DATA64_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax ; Or set FS/GS to 0 or specific values for kernel/user later
    mov gs, ax
    ; SS was loaded by IRETQ from the stack
    ret
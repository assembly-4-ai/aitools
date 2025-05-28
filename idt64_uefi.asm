; idt64_uefi.asm: IDT Setup for Exceptions + IRQs (post-UEFI)
; Depends on: boot_defs_temp.inc

BITS 64
%include "boot_defs_temp.inc"

global setup_final_idt64
global load_idt64
global idt64_pointer_var ; Variable holding IDTR value
extern panic64           ; For common ISR handler
extern keyboard_interrupt_handler ; Specific IRQ handler
extern ahci_irq_handler         ; Specific IRQ handler
; extern lapic_send_eoi       ; If using APIC and EOI is there

struc Idt64Entry
    .OffsetLow      resw 1
    .Selector       resw 1
    .IST            resb 1  ; Interrupt Stack Table index
    .TypeAttr       resb 1
    .OffsetMid      resw 1
    .OffsetHigh     resd 1
    .Reserved       resd 1
endstruc
Idt64Entry_size equ 16

; Gate Types
IDT_TYPE_INT_GATE   equ 0x8E ; P=1, DPL=0, Type=0xE
IDT_TYPE_TRAP_GATE  equ 0x8F ; P=1, DPL=0, Type=0xF

section .data align=8
idt64_pointer_var:
    dw (IDT_MAX_ENTRIES * Idt64Entry_size - 1) ; Limit (pre-calculated)
    dq 0 ; Base (set at runtime)

isr_panic_msg db "Unhandled Exception! ISR: 0x", 0
irq_panic_msg db "Unhandled IRQ! Vector: 0x", 0

; --- ISR Stub Table (0-31) ---
isr_stub_table:
    dq isr0_stub
    dq isr1_stub
    dq isr2_stub
    dq isr3_stub
    dq isr4_stub
    dq isr5_stub
    dq isr6_stub
    dq isr7_stub
    dq isr8_stub
    dq isr9_stub
    dq isr10_stub
    dq isr11_stub
    dq isr12_stub
    dq isr13_stub
    dq isr14_stub
    dq isr15_stub
    dq isr16_stub
    dq isr17_stub
    dq isr18_stub
    dq isr19_stub
    dq isr_generic_stub ; 20
    dq isr_generic_stub ; 21
    dq isr_generic_stub ; 22
    dq isr_generic_stub ; 23
    dq isr_generic_stub ; 24
    dq isr_generic_stub ; 25
    dq isr_generic_stub ; 26
    dq isr_generic_stub ; 27
    dq isr_generic_stub ; 28
    dq isr_generic_stub ; 29
    dq isr_generic_stub ; 30
    dq isr_generic_stub ; 31

; Attributes for first 20 exceptions
isr_attrs:
    db IDT_TYPE_INT_GATE  ; 0
    db IDT_TYPE_TRAP_GATE ; 1
    db IDT_TYPE_INT_GATE  ; 2
    db IDT_TYPE_TRAP_GATE ; 3
    db IDT_TYPE_TRAP_GATE ; 4
    db IDT_TYPE_INT_GATE  ; 5
    db IDT_TYPE_INT_GATE  ; 6
    db IDT_TYPE_INT_GATE  ; 7
    db IDT_TYPE_INT_GATE  ; 8 (Error code pushed by CPU)
    db IDT_TYPE_INT_GATE  ; 9
    db IDT_TYPE_INT_GATE  ; 10 (Error code pushed by CPU)
    db IDT_TYPE_INT_GATE  ; 11 (Error code pushed by CPU)
    db IDT_TYPE_INT_GATE  ; 12 (Error code pushed by CPU)
    db IDT_TYPE_INT_GATE  ; 13 (Error code pushed by CPU)
    db IDT_TYPE_INT_GATE  ; 14 (Error code pushed by CPU)
    db IDT_TYPE_INT_GATE  ; 15
    db IDT_TYPE_INT_GATE  ; 16
    db IDT_TYPE_INT_GATE  ; 17 (Error code pushed by CPU)
    db IDT_TYPE_INT_GATE  ; 18
    db IDT_TYPE_INT_GATE  ; 19
isr_ist:
    db 0, 0, 1, 0, 0, 0, 0, 0, 1, 0 ; IST 1 for NMI(2), DF(8)
    db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ; IST 0 for others

; --- IRQ Stub Table (Vectors PIC1_IRQ_START to PIC1_IRQ_START+15) ---
; This table provides the actual handler addresses for IRQs.
; Vector = PIC1_IRQ_START + IRQ_Number
irq_handler_table:
    dq irq_timer_stub_entry          ; IRQ 0  (Vector 0x20) - Timer
    dq keyboard_interrupt_handler    ; IRQ 1  (Vector 0x21) - Keyboard
    dq irq_cascade_stub_entry        ; IRQ 2  (Vector 0x22) - PIC Cascade
    dq irq_com2_stub_entry           ; IRQ 3  (Vector 0x23) - COM2
    dq irq_com1_stub_entry           ; IRQ 4  (Vector 0x24) - COM1
    dq irq_lpt2_stub_entry           ; IRQ 5  (Vector 0x25) - LPT2
    dq irq_floppy_stub_entry         ; IRQ 6  (Vector 0x26) - Floppy
    dq irq_lpt1_stub_entry           ; IRQ 7  (Vector 0x27) - LPT1 / Spurious
    dq irq_rtc_stub_entry            ; IRQ 8  (Vector 0x28) - CMOS RTC
    dq irq_free9_stub_entry          ; IRQ 9  (Vector 0x29) - Free / ACPI SCI
    dq ahci_irq_handler              ; IRQ 10 (Vector 0x2A) - Example for AHCI on IRQ10
    dq irq_free11_stub_entry         ; IRQ 11 (Vector 0x2B) - Free
    dq irq_mouse_stub_entry          ; IRQ 12 (Vector 0x2C) - PS/2 Mouse
    dq irq_fpu_stub_entry            ; IRQ 13 (Vector 0x2D) - FPU Coprocessor
    dq ahci_irq_handler              ; IRQ 14 (Vector 0x2E) - Example for AHCI on IRQ14 (Primary ATA default)
    dq irq_free15_stub_entry         ; IRQ 15 (Vector 0x2F) - Free (Secondary ATA default)

section .text

%macro SET_IDT_ENTRY_MACRO 5 ; Args: EntryPtr(RSI), StubAddr(RAX), Selector(imm), TypeAttr(BL), IST(DL)
    mov word [rsi + Idt64Entry.OffsetLow], ax
    mov word [rsi + Idt64Entry.Selector], %3
    mov byte [rsi + Idt64Entry.IST], dl
    mov byte [rsi + Idt64Entry.TypeAttr], bl
    shr rax, 16
    mov word [rsi + Idt64Entry.OffsetMid], ax
    shr rax, 16
    mov dword [rsi + Idt64Entry.OffsetHigh], eax
    mov dword [rsi + Idt64Entry.Reserved], 0
%endmacro

;--------------------------------------------------------------------------
; setup_final_idt64: Sets up IDT entries for ISRs 0-31 and IRQs
; Input: RDI = Physical address of allocated memory for IDT
;--------------------------------------------------------------------------
setup_final_idt64:
    push rdi ; Save IDT base address

    mov rcx, IDT_MAX_ENTRIES * Idt64Entry_size
    xor al, al
    rep stosb ; Zero IDT

    pop rdi ; Restore IDT base address
    mov rsi, rdi ; RSI = Current IDT entry pointer

    ; Setup exceptions 0-31
    xor rcx, rcx ; ISR number / table index
.isr_loop_sfi:
    cmp rcx, 32
    jae .setup_irqs_sfi

    mov rax, [isr_stub_table + rcx*8]
    cmp rcx, 20
    jae .use_generic_attr_sfi
    mov bl, [isr_attrs + rcx]
    mov dl, [isr_ist + rcx]
    jmp .set_isr_entry_sfi
.use_generic_attr_sfi:
    mov bl, IDT_TYPE_INT_GATE
    xor dl, dl
.set_isr_entry_sfi:
    SET_IDT_ENTRY_MACRO rsi, rax, CODE64_SEL, bl, dl
    add rsi, Idt64Entry_size
    inc rcx
    jmp .isr_loop_sfi

.setup_irqs_sfi:
    ; RSI should now point to IDT[32] (vector 0x20)
    xor rcx, rcx ; IRQ index (0-15)
.irq_loop_sfi:
    cmp rcx, 16 ; Setup 16 IRQs (0x20 to 0x2F)
    jae .idt_setup_done_sfi

    mov rax, [irq_handler_table + rcx*8] ; Get specific or generic IRQ handler address
    mov bl, IDT_TYPE_INT_GATE ; All IRQs are Interrupt Gates
    xor dl, dl ; IST = 0 for IRQs
    SET_IDT_ENTRY_MACRO rsi, rax, CODE64_SEL, bl, dl

    add rsi, Idt64Entry_size
    inc rcx
    jmp .irq_loop_sfi

.idt_setup_done_sfi:
    ; Update IDTR variable
    mov rax, rdi ; IDT Base address
    ; Limit is already set in .data for idt64_pointer_var
    mov [idt64_pointer_var + 2], rax ; Base

    ret

;--------------------------------------------------------------------------
; load_idt64: Loads the IDT register (LIDT)
;--------------------------------------------------------------------------
load_idt64:
    lidt [idt64_pointer_var]
    ret

; --- ISR Stubs ---
%macro ISR_STUB_MACRO 2-3 0 ; Params: ISR_Num, PushErrorCode (0 or 1), ErrorCodeValue (optional)
isr%1_stub:
    %if %2 == 0
        push qword %3 ; Push dummy error code
    %endif
    push %1           ; Push ISR number
    jmp common_isr_handler
%endmacro

ISR_STUB_MACRO 0, 0, 0
ISR_STUB_MACRO 1, 0, 0
ISR_STUB_MACRO 2, 0, 0
ISR_STUB_MACRO 3, 0, 0
ISR_STUB_MACRO 4, 0, 0
ISR_STUB_MACRO 5, 0, 0
ISR_STUB_MACRO 6, 0, 0
ISR_STUB_MACRO 7, 0, 0
ISR_STUB_MACRO 8, 1    ; CPU Pushes Error Code
ISR_STUB_MACRO 9, 0, 0
ISR_STUB_MACRO 10, 1   ; CPU Pushes Error Code
ISR_STUB_MACRO 11, 1   ; CPU Pushes Error Code
ISR_STUB_MACRO 12, 1   ; CPU Pushes Error Code
ISR_STUB_MACRO 13, 1   ; CPU Pushes Error Code
ISR_STUB_MACRO 14, 1   ; CPU Pushes Error Code
ISR_STUB_MACRO 15, 0, -1 ; Reserved
ISR_STUB_MACRO 16, 0, 0
ISR_STUB_MACRO 17, 1   ; CPU Pushes Error Code
ISR_STUB_MACRO 18, 0, 0
ISR_STUB_MACRO 19, 0, 0

isr_generic_stub: ; For ISR 20-31 and unassigned IRQs
    push qword 0 ; Dummy error code
    push qword -1 ; Placeholder for vector number (actual vector not easily known here)
    jmp common_isr_handler

; --- Specific IRQ Stubs that jump to common_irq_handler if not handled directly ---
; If an IRQ handler (like keyboard_interrupt_handler) does its own context save/restore and iretq,
; it's pointed to directly from irq_handler_table.
; Stubs below are for IRQs that might use a common wrapper before specific C code, or for generic handling.
irq_timer_stub_entry:       push qword 0; push qword (PIC1_IRQ_START + 0); jmp common_irq_handler
irq_cascade_stub_entry:     push qword 0; push qword (PIC1_IRQ_START + 2); jmp common_irq_handler
irq_com2_stub_entry:        push qword 0; push qword (PIC1_IRQ_START + 3); jmp common_irq_handler
irq_com1_stub_entry:        push qword 0; push qword (PIC1_IRQ_START + 4); jmp common_irq_handler
irq_lpt2_stub_entry:        push qword 0; push qword (PIC1_IRQ_START + 5); jmp common_irq_handler
irq_floppy_stub_entry:      push qword 0; push qword (PIC1_IRQ_START + 6); jmp common_irq_handler
irq_lpt1_stub_entry:        push qword 0; push qword (PIC1_IRQ_START + 7); jmp common_irq_handler
irq_rtc_stub_entry:         push qword 0; push qword (PIC1_IRQ_START + 8); jmp common_irq_handler
irq_free9_stub_entry:       push qword 0; push qword (PIC1_IRQ_START + 9); jmp common_irq_handler
; IRQ 10 (AHCI) uses direct handler: ahci_irq_handler
irq_free11_stub_entry:      push qword 0; push qword (PIC1_IRQ_START + 11); jmp common_irq_handler
irq_mouse_stub_entry:       push qword 0; push qword (PIC1_IRQ_START + 12); jmp common_irq_handler
irq_fpu_stub_entry:         push qword 0; push qword (PIC1_IRQ_START + 13); jmp common_irq_handler
; IRQ 14 (AHCI) uses direct handler: ahci_irq_handler
irq_free15_stub_entry:      push qword 0; push qword (PIC1_IRQ_START + 15); jmp common_irq_handler


;--------------------------------------------------------------------------
common_isr_handler:
    ; Save all general purpose registers
    push r15; push r14; push r13; push r12
    push r11; push r10; push r9;  push r8
    push rbp; push rdi; push rsi
    push rdx; push rcx; push rbx; push rax

    ; Stack: RAX,RBX,...,R15, ISR_Num, ErrorCode, RIP_iret, CS_iret, RFLAGS_iret, RSP_iret, SS_iret
    mov rdi, [rsp + 15*8 + 16] ; ISR Number
    mov rsi, [rsp + 15*8 + 8]  ; Error Code
    call panic64 ; panic64(isr_num, error_code)

.halt_isr_cih:
    cli
    hlt
    jmp .halt_isr_cih

;--------------------------------------------------------------------------
common_irq_handler: ; For IRQs that don't have a direct, full handler
    ; Save all general purpose registers
    push r15; push r14; push r13; push r12
    push r11; push r10; push r9;  push r8
    push rbp; push rdi; push rsi
    push rdx; push rcx; push rbx; push rax

    mov rdi, [rsp + 15*8 + 16] ; Vector Number (IRQ Num + Base)

    ; --- Send EOI ---
    ; This depends on whether PIC or APIC is active.
    ; Assuming APIC, LAPIC EOI is needed.
    ; If PIC, need to check if IRQ > 7 for slave PIC EOI.
    ; call lapic_send_eoi ; If APIC is primary controller
    ; If PIC:
    mov r8, rdi ; Save vector
    sub rdi, PIC1_IRQ_START ; Convert vector to IRQ number (0-15)
    cmp dil, 8
    jae .eoi_slave_cirq
.eoi_master_cirq:
    mov al, PIC_EOI
    out PIC1_COMMAND, al
    jmp .eoi_done_cirq
.eoi_slave_cirq:
    mov al, PIC_EOI
    out PIC2_COMMAND, al
    out PIC1_COMMAND, al ; Also to master
.eoi_done_cirq:
    mov rdi, r8 ; Restore vector for panic message

    ; Panic for unhandled IRQs that fall through here
    mov rsi, irq_panic_msg
    call panic64 ; panic64(vector_num, "Unhandled IRQ!...")

.halt_irq_cih:
    cli
    hlt
    jmp .halt_irq_cih
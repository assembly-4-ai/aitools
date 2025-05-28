BITS 64
%include "boot_defs_temp.inc"

global acpi_init
; extern UefiPrint ; If debug messages are needed later for UefiPrint calls
extern memcpy64 ; For copying IRQ override entries
; extern memset64 ; If needed for more complex operations

section .data align=8
    gRsdpAddress dq 0
    gAcpiSdtAddress dq 0
    gAcpiSdtIsXsdt db 0
    acpi_rsdp_signature db "RSD PTR ", 0 ; 8 bytes + null for safety

    ; Optional debug messages (can be used with a UefiPrint-like function if ported to post-BS)
    ; rsdp_search_msg db "Searching for RSDP...", 0Dh, 0Ah, 0
    ; rsdp_found_ebda_msg db "RSDP found in EBDA at 0x", 0
    ; rsdp_found_rom_msg db "RSDP found in ROM at 0x", 0
    ; rsdp_not_found_msg db "RSDP not found.", 0Dh, 0Ah, 0
    ; rsdp_checksum_ok_msg db "RSDP Checksum OK.", 0Dh, 0Ah, 0
    ; rsdp_checksum_fail_msg db "RSDP Checksum FAILED!", 0Dh, 0Ah, 0
    ; rsdp_ext_checksum_ok_msg db "RSDP Ext Checksum OK.", 0Dh, 0Ah, 0
    ; rsdp_ext_checksum_fail_msg db "RSDP Ext Checksum FAILED!", 0Dh, 0Ah, 0

    gLocalApicAddress_acpi dq 0  ; Physical address of the Local APIC
    gIoApicAddress_acpi dq 0     ; Physical address of the primary I/O APIC
    gIoApicId_acpi db 0          ; ID of the primary I/O APIC
    gIoApicBaseGsi_acpi dd 0     ; Global System Interrupt Base for the primary I/O APIC
    gNumCpuCores_acpi dd 0       ; Number of CPU cores found (enabled LAPICs)
    MAX_IRQ_OVERRIDES equ 16
    gNumIrqOverrides_acpi db 0
    gNumaNodeCount_acpi dd 0       ; Number of NUMA memory regions found
    MAX_NUMA_NODES equ 16          ; Max memory regions/nodes we'll track from SRAT
    gPciEcamBaseAddress_acpi dq 0  ; Stores BaseAddress for PCI Segment Group 0

section .bss align=8
    gIrqOverrides_acpi resb (ACPI_MADT_INT_OVERRIDE_size * MAX_IRQ_OVERRIDES)
    ; Structure for storing parsed NUMA memory region info
    struc NUMA_NODE_MEM_INFO_acpi_struc ; Renamed to avoid conflict if NUMA_NODE_MEM_INFO is global
        .BaseAddress    resq 1
        .Length         resq 1
        .ProximityDomain resd 1
        .Flags          resd 1 ; From SRAT Memory Affinity Flags entry
    endstruc
    NUMA_NODE_MEM_INFO_acpi_size equ NUMA_NODE_MEM_INFO_acpi_struc.size ; Should be 8+8+4+4 = 24
    gNumaNodeInfo_acpi resb (NUMA_NODE_MEM_INFO_acpi_size * MAX_NUMA_NODES)

section .text

; Helper function to calculate checksum of a memory block.
; Input: RDI = start address, RCX = length in bytes.
; Output: AL = sum of all bytes (mod 256). Checksum is valid if AL is 0.
calculate_checksum:
    push rsi
    push rcx
    push rdi
    xor al, al          ; Accumulator for sum
    mov rsi, rdi        ; RSI = current address to read from
.checksum_loop_cs:
    cmp rcx, 0
    jz .checksum_done_cs
    add al, [rsi]       ; Add byte to sum
    inc rsi             ; Next byte
    dec rcx             ; Decrement count
    jmp .checksum_loop_cs
.checksum_done_cs:
    pop rdi
    pop rcx
    pop rsi
    ret

; Helper function to search a memory range for the RSDP signature.
; Input: RDI = start_address, RSI = end_address (exclusive).
; Output: RAX = address of RSDP if found, or 0 if not found.
; Clobbers: RDI, RSI, RCX, RDX, R10, R11. Preserves others via push/pop if needed by caller.
search_rsdp_in_range:
    push rbx ; RBX is callee-saved, others are scratch or args
    mov r10, rdi ; R10 = current search pointer
    mov r11, rsi ; R11 = end_address
.search_loop_srir:
    cmp r10, r11
    jge .search_not_found_srir ; Reached end of range or range invalid

    ; Compare signature "RSD PTR " (8 bytes)
    push rdi ; Save rdi for cmpsb
    push rsi ; Save rsi for cmpsb
    push rcx ; Save rcx for cmpsb
    lea rdi, [rel acpi_rsdp_signature]
    mov rsi, r10 ; Address to check
    mov ecx, 8   ; Length of signature
    repe cmpsb   ; Compare bytes
    pop rcx
    pop rsi
    pop rdi
    jne .search_next_addr_srir ; Signature mismatch

    ; Signature matched, current R10 is the address.
    mov rax, r10
    jmp .search_found_srir

.search_next_addr_srir:
    add r10, 16 ; Move to next 16-byte aligned address
    jmp .search_loop_srir

.search_not_found_srir:
    xor rax, rax ; Return 0 if not found
.search_found_srir:
    pop rbx
    ret

acpi_init: ; Returns EFI_STATUS in RAX (0 for success, non-zero for error)
    pushaq
    mov qword [gRsdpAddress], 0
    mov qword [gAcpiSdtAddress], 0
    mov byte [gAcpiSdtIsXsdt], 0

    ; --- Stage 1: Search EBDA ---
    ; Get EBDA base address: word at BDA 0x40E contains segment of EBDA.
    ; Physical address = segment * 16.
    xor rax, rax ; Clear RAX before reading word
    mov ax, [0x40E] ; AX = EBDA segment
    shl rax, 4      ; RAX = EBDA physical base address

    ; Check if EBDA address is plausible (e.g., not 0, within reasonable limits)
    test rax, rax
    jz .try_search_rom ; If EBDA address is 0, skip EBDA search

    mov rdi, rax    ; Start of EBDA search range
    mov rsi, rax
    add rsi, 1024   ; Search first 1KB of EBDA
    call search_rsdp_in_range
    test rax, rax
    jnz .rsdp_candidate_found ; Found a candidate in EBDA

.try_search_rom:
    ; --- Stage 2: Search BIOS ROM (0xE0000 to 0xFFFFF) ---
    mov rdi, BIOS_ROM_SEARCH_START
    mov rsi, BIOS_ROM_SEARCH_END + 1 ; search_rsdp_in_range uses exclusive end address
    call search_rsdp_in_range
    test rax, rax
    jz .rsdp_final_not_found_exit ; Not found in ROM either, definitive not found

    ; Fall through to rsdp_candidate_found if found in ROM

.rsdp_candidate_found:
    ; RAX contains the address of the potential RSDP structure
    mov rbx, rax ; RBX = Potential RSDP address

    ; Validate Checksum for RSDP V1 part (first 20 bytes)
    mov rdi, rbx ; Address of RSDP
    mov ecx, RSDP_DESCRIPTOR_V1_SIZE ; Length for V1 checksum (20 bytes)
    call calculate_checksum
    test al, al ; Checksum byte should be 0 for valid
    jnz .rsdp_validation_failed ; Checksum failed, this candidate is invalid

    ; Checksum V1 OK. Store RSDP address.
    mov [gRsdpAddress], rbx

    ; Check Revision to determine if it's V2+ and if XSDT might be used.
    mov cl, [rbx + RSDP_DESCRIPTOR_V1.Revision] ; Get Revision byte
    cmp cl, 2
    jl .rsdp_use_rsdt ; If revision < 2, it's strictly RSDP v1, use RSDT

    ; Revision is 2 or greater. Validate Extended Checksum for V2 structure.
    ; Length for V2 checksum is in the 'Length' field of RSDP V2.
    mov ecx, [rbx + RSDP_DESCRIPTOR_V2.Length] ; ECX = Full length for V2 checksum
    test ecx, ecx ; Ensure length is not zero
    jz .rsdp_validation_failed ; Invalid length for V2
    cmp ecx, RSDP_DESCRIPTOR_V1_SIZE ; Length must be at least V1 size
    jle .rsdp_validation_failed ; If length <= V1 size but rev >= 2, it's inconsistent

    mov rdi, rbx ; Address of RSDP
    call calculate_checksum ; AL = checksum byte
    test al, al
    jnz .rsdp_validation_failed ; Extended checksum failed

    ; Extended Checksum V2 OK. Prefer XSDT if address is non-NULL.
    mov rax, [rbx + RSDP_DESCRIPTOR_V2.XsdtAddress]
    test rax, rax
    jnz .rsdp_use_xsdt_addr ; If XSDT address is non-zero, use it

    ; XSDT address is NULL or not V2, fall back to RSDT from V1 part.
.rsdp_use_rsdt:
    mov eax, [rbx + RSDP_DESCRIPTOR_V1.RsdtAddress] ; EAX = RSDT address
    mov [gAcpiSdtAddress], rax
    mov byte [gAcpiSdtIsXsdt], 0 ; Mark as RSDT
    jmp .rsdp_found_and_sdt_chosen

.rsdp_use_xsdt_addr:
    ; XSDT address is in RAX from the read above
    mov [gAcpiSdtAddress], rax
    mov byte [gAcpiSdtIsXsdt], 1 ; Mark as XSDT
    ; Fall through to rsdp_found_and_sdt_chosen

.rsdp_found_and_sdt_chosen:
    ; At this point, gAcpiSdtAddress and gAcpiSdtIsXsdt are set.
    ; Now, validate the SDT header itself.
    mov rsi, [gAcpiSdtAddress]
    test rsi, rsi ; Check if SDT address is NULL
    jz .sdt_validation_failed_not_found ; Should not happen if RSDP parsing was correct, but defensive

    ; Validate Signature of the SDT (RSDT or XSDT)
    mov edx, [rsi + ACPI_TABLE_HEADER.Signature] ; EDX = Table Signature from header
    mov cl, byte [gAcpiSdtIsXsdt]
    test cl, cl ; Is it RSDT (0) or XSDT (1)?
    jz .validate_rsdt_sig_acpi
.validate_xsdt_sig_acpi:
    cmp edx, ACPI_SIG_XSDT
    je .sdt_sig_ok_acpi
    jmp .sdt_validation_failed_compromised ; Signature mismatch for XSDT
.validate_rsdt_sig_acpi:
    cmp edx, ACPI_SIG_RSDT
    je .sdt_sig_ok_acpi
    jmp .sdt_validation_failed_compromised ; Signature mismatch for RSDT
.sdt_sig_ok_acpi:

    ; Validate Checksum of the SDT
    mov ecx, [rsi + ACPI_TABLE_HEADER.Length] ; ECX = Table Length from header
    cmp ecx, ACPI_TABLE_HEADER_SIZE           ; Length must be at least header size
    jl .sdt_validation_failed_compromised     ; Invalid length

    mov rdi, rsi ; RDI = Table Address for checksum
    ; RCX (length) is already set
    call calculate_checksum ; AL = sum of bytes
    test al, al ; Checksum result in AL, should be 0
    jnz .sdt_validation_failed_crc ; Checksum failed

    ; All SDT header validations passed
    mov rax, EFI_SUCCESS
    jmp .acpi_init_exit_label

.sdt_validation_failed_not_found:
    mov rax, EFI_NOT_FOUND ; Indicates SDT pointer itself was bad
    jmp .acpi_init_exit_label

.sdt_validation_failed_compromised:
    mov rax, EFI_COMPROMISED_DATA ; Bad signature or length
    jmp .acpi_init_exit_label

.sdt_validation_failed_crc:
    mov rax, EFI_CRC_ERROR ; Checksum error on SDT
    jmp .acpi_init_exit_label

.rsdp_validation_failed:
    ; This candidate RSDP failed validation.
    ; The current simplified logic means if the first candidate found in EBDA is invalid,
    ; it will jump to .try_search_rom. If a candidate from ROM is invalid, it will jump here
    ; and then to .rsdp_final_not_found_exit.
    ; If the EBDA search yielded no candidate initially (rax was 0), it also goes to .try_search_rom.
    ; This means that if an EBDA candidate fails validation, the ROM search *will* be attempted.
    ; This is implicitly handled by where .rsdp_candidate_found is jumped from.
    ; If .rsdp_candidate_found was reached from EBDA search and validation fails:
    ;   The next step should be to attempt ROM search if not already done.
    ;   The current code: If EBDA search found candidate (RAX!=0), it goes to .rsdp_candidate_found.
    ;   If validation fails there, it goes to .rsdp_validation_failed, then to .rsdp_final_not_found_exit.
    ;   This is the simplification: it doesn't re-enter the search stages from .rsdp_validation_failed.
    ;   It relies on the initial search calls to chain correctly.
    ; The current logic is:
    ;   Search EBDA. Found? -> Validate. Valid? -> Done. Invalid? -> Search ROM.
    ;                  Not Found? -> Search ROM.
    ;   Search ROM. Found? -> Validate. Valid? -> Done. Invalid? -> Fail.
    ;                  Not Found? -> Fail.
    ; This is correctly implemented by the jumps. If EBDA validation fails (`jnz .rsdp_validation_failed`),
    ; it means `acpi_init` will effectively proceed as if EBDA search didn't yield a *valid* RSDP,
    ; and the previous logic should ensure ROM search is then attempted if the first path was EBDA.
    ; Re-evaluating the provided code's flow:
    ; - Search EBDA. if found, jnz .rsdp_candidate_found. (OK)
    ; - If not found in EBDA, then .try_search_rom is hit. Search ROM. if found, jnz .rsdp_candidate_found (implicit fallthrough). if not, jz .rsdp_final_not_found_exit. (OK)
    ; - .rsdp_candidate_found:
    ;   - Validates. If validation fails (e.g. bad checksum), jnz .rsdp_validation_failed.
    ; - .rsdp_validation_failed: (THIS IS THE CRITICAL POINT)
    ;   - jmp .rsdp_final_not_found_exit
    ; This means if an EBDA candidate *fails validation*, it exits as NOT_FOUND without trying ROM. This is the flaw.
    ; The user's note "The logic for continuing search ... is simplified" refers to this.
    ; I must use the code AS PROVIDED by the user.
    jmp .rsdp_final_not_found_exit ; Per user's simplified code structure

.rsdp_final_not_found_exit:
    mov rax, EFI_NOT_FOUND

.acpi_init_exit_label:
    popaq
    ret

global acpi_parse_mcfg
acpi_parse_mcfg: ; Parses MCFG. Returns EFI_STATUS in RAX.
    pushaq
    mov qword [gPciEcamBaseAddress_acpi], 0 ; Initialize to 0

    ; Find the MCFG table
    mov edi, ACPI_SIG_MCFG ; 'MCFG'
    call acpi_find_table
    jc .apmf_exit_error      ; CF set if not found or error in acpi_find_table
    test rax, rax            ; RAX = MCFG address
    jz .apmf_not_found       ; If address is 0, also not found

    ; MCFG found: RAX = MCFG address, RBX = MCFG length
    mov r12, rax ; R12 = MCFG physical address
    mov r13, rbx ; R13 = MCFG total length

    ; RSI = Pointer to the first MCFG allocation structure
    ; This is after ACPI_TABLE_HEADER (36 bytes) and one reserved QWORD (8 bytes)
    mov rsi, r12
    add rsi, ACPI_TABLE_HEADER_SIZE
    add rsi, 8 ; Skip reserved QWORD (ACPI_MCFG_TABLE_HEADER.Reserved)

    ; RCX = Remaining length for allocation structures
    mov ecx, r13d                        ; Total MCFG length
    sub ecx, ACPI_TABLE_HEADER_SIZE      ; Subtract ACPI_TABLE_HEADER
    sub ecx, 8                           ; Subtract reserved QWORD

.apmf_entry_loop:
    cmp ecx, ACPI_MCFG_ALLOCATION_STRUCTURE_SIZE ; Enough space for one allocation structure?
    jl .apmf_parse_done ; If not, done parsing

    ; RBP can be used to point to the current allocation structure (RSI)
    mov rbp, rsi

    ; Read PCISegmentGroup
    movzx eax, word [rbp + ACPI_MCFG_ALLOCATION_STRUCTURE.PCISegmentGroup]
    cmp eax, 0 ; We are interested in Segment Group 0
    jne .apmf_next_entry ; If not segment 0, skip to next entry

    ; Found Segment Group 0, store its BaseAddress
    mov rax, [rbp + ACPI_MCFG_ALLOCATION_STRUCTURE.BaseAddress]
    mov [gPciEcamBaseAddress_acpi], rax
    ; For now, we only care about segment 0, so we can stop parsing.
    ; If multiple segments were to be supported, remove the jmp and store in an array.
    jmp .apmf_parse_done_success

.apmf_next_entry:
    add rsi, ACPI_MCFG_ALLOCATION_STRUCTURE_SIZE
    sub ecx, ACPI_MCFG_ALLOCATION_STRUCTURE_SIZE
    jmp .apmf_entry_loop

.apmf_parse_done_success:
    mov rax, EFI_SUCCESS
    jmp .apmf_exit

.apmf_parse_done:
    ; Reached end of table. If gPciEcamBaseAddress_acpi is still 0, segment 0 was not found.
    ; Still, parsing the table itself was "successful" in that we processed what was there.
    mov rax, EFI_SUCCESS
    jmp .apmf_exit

.apmf_not_found:
    mov rax, EFI_NOT_FOUND
    jmp .apmf_exit_error_common

.apmf_exit_error:
    ; CF is already set by acpi_find_table or error occurred here
    test rax, rax
    jnz .apmf_exit ; Propagate error from acpi_find_table
.apmf_exit_error_common:
    stc ; Ensure CF is set
    test rax, rax
    jnz .apmf_exit
    mov rax, EFI_DEVICE_ERROR ; Generic error if specific one not set
.apmf_exit:
    popaq
    ret

global acpi_find_table
acpi_find_table: ; RDI=Signature (dword) -> RAX=TablePtr, RBX=TableLength, CF on error/not found
    pushaq
    mov r12d, edi ; Save target signature (ensure only lower 32 bits of RDI are used for signature)

    mov rsi, [gAcpiSdtAddress]
    test rsi, rsi
    jz .aft_error_no_sdt ; ACPI not initialized or SDT address is NULL

    mov r10d, [rsi + ACPI_TABLE_HEADER.Length] ; R10D = Total length of XSDT/RSDT
    cmp r10d, ACPI_TABLE_HEADER_SIZE
    jle .aft_error_bad_sdt ; SDT length too small or just header

    sub r10d, ACPI_TABLE_HEADER_SIZE ; R10D = Length of pointer area
    mov r11, rsi
    add r11, ACPI_TABLE_HEADER_SIZE ; R11 = Pointer to first entry in XSDT/RSDT

    movzx r13, byte [gAcpiSdtIsXsdt] ; R13 = 0 for RSDT, 1 for XSDT
    test r13b, r13b ; is XSDT? (check if R13 is non-zero)
    jnz .aft_xsdt_entry_processing_loop

.aft_rsdt_entry_processing_loop: ; Handle RSDT (dword entries)
    cmp r10d, 4 ; Enough bytes remaining for one dword entry?
    jl .aft_not_found ; No more entries to process

    mov eax, [r11] ; EAX = Physical address of child table (32-bit for RSDT)
    xor r14, r14   ; Clear R14 before moving 32-bit address into its lower part
    mov r14d, eax  ; R14 = Child table address (zero-extended to 64-bit)
    add r11, 4     ; Next entry
    sub r10d, 4    ; Decrement remaining length of pointer area
    jmp .aft_process_child_table

.aft_xsdt_entry_processing_loop: ; Handle XSDT (qword entries)
    cmp r10d, 8 ; Enough bytes remaining for one qword entry?
    jl .aft_not_found ; No more entries to process

    mov r14, [r11] ; R14 = Physical address of child table (64-bit for XSDT)
    add r11, 8     ; Next entry
    sub r10d, 8    ; Decrement remaining length of pointer area
    ; Fall through to .aft_process_child_table

.aft_process_child_table:
    test r14, r14 ; Is child table address NULL?
    jz .aft_next_entry_loop_decision ; Skip if NULL, process next entry

    ; R14 points to the child table header. Read its length and signature.
    mov edi, r14 ; RDI for calculate_checksum (child_table_addr)
    mov ecx, [r14 + ACPI_TABLE_HEADER.Length] ; ECX = Length of child table
    cmp ecx, ACPI_TABLE_HEADER_SIZE ; Basic validation: length must be at least header size
    jl .aft_next_entry_loop_decision ; Child table length too small, skip

    push rcx ; Save child table length for later if needed (for RBX)
    call calculate_checksum ; Checksum of child table. RDI, RCX are args. Result in AL.
    pop rcx  ; Restore child table length
    test al, al ; Checksum AL should be 0
    jnz .aft_next_entry_loop_decision ; Checksum failed for child table, skip it

    ; Checksum OK, now compare signature
    mov eax, [r14 + ACPI_TABLE_HEADER.Signature] ; EAX = Signature of child table
    cmp eax, r12d ; Compare with target signature (from R12D)
    jne .aft_next_entry_loop_decision ; Signatures do not match, skip

    ; Found matching table with good checksum!
    mov rax, r14 ; Return table address in RAX
    mov ebx, ecx ; Return table length in RBX (length was in ECX)
    clc          ; Clear Carry Flag (success)
    jmp .aft_exit

.aft_next_entry_loop_decision:
    test r13b, r13b ; is XSDT? (was it non-zero)
    jnz .aft_xsdt_entry_processing_loop
    jmp .aft_rsdt_entry_processing_loop ; else RSDT loop

.aft_error_no_sdt:
.aft_error_bad_sdt:
.aft_not_found:
    xor rax, rax ; Return 0 for address
    xor rbx, rbx ; Return 0 for length
    stc          ; Set Carry Flag (error or not found)
.aft_exit:
    popaq
    ret

; Defines for MADT entry struct sizes (must match boot_defs_temp.inc if defined there too)
; If not in boot_defs_temp.inc, define them here. For now, assume they are in boot_defs_temp.inc
; or ensure the offsets used directly in acpi_parse_madt are correct.
; %define ACPI_MADT_LOCAL_APIC_size 6 ; AcpiProcId_1 + ApicId_1 + Flags_4 (struc size is 8 due to alignment in boot_defs_temp.inc, but data is packed)
; %define ACPI_MADT_IO_APIC_size 8    ; IoApicId_1 + Reserved_1 + IoApicAddr_4 + GSIBase_4 (struc size is 12)
; %define ACPI_MADT_INT_OVERRIDE_size 8 ; Bus_1 + Source_1 + GSI_4 + Flags_2 (struc size is 10)
; Using direct offsets from structures defined in boot_defs_temp.inc is safer.

global acpi_parse_madt
acpi_parse_madt: ; Parses MADT. Returns EFI_STATUS in RAX.
    pushaq
    ; Initialize MADT specific globals
    mov dword [gNumCpuCores_acpi], 0
    mov byte [gNumIrqOverrides_acpi], 0
    mov qword [gIoApicAddress_acpi], 0 ; Clear to ensure we only take the first I/O APIC found

    ; Find the MADT table
    mov edi, ACPI_SIG_MADT ; 'APIC'
    call acpi_find_table
    jc .apm_exit_error      ; CF set if not found or error in acpi_find_table
    test rax, rax           ; RAX = MADT address
    jz .apm_not_found       ; If address is 0, also not found

    ; MADT found: RAX = MADT address, RBX = MADT length
    mov r12, rax ; R12 = MADT physical address
    mov r13, rbx ; R13 = MADT total length

    ; Store Local APIC Address (from MADT header part)
    mov eax, [r12 + ACPI_TABLE_HEADER_SIZE + ACPI_MADT_TABLE_HEADER.LocalApicAddress]
    mov [gLocalApicAddress_acpi], rax

    ; RSI = Pointer to the first MADT entry
    mov rsi, r12
    add rsi, ACPI_TABLE_HEADER_SIZE      ; Skip ACPI_TABLE_HEADER
    add rsi, ACPI_MADT_TABLE_HEADER_SIZE ; Skip MADT-specific header fields (LocalApicAddress, Flags)

    ; RCX = Remaining length of MADT entries
    mov ecx, r13d                        ; Total MADT length
    sub ecx, ACPI_TABLE_HEADER_SIZE      ; Subtract ACPI_TABLE_HEADER
    sub ecx, ACPI_MADT_TABLE_HEADER_SIZE ; Subtract MADT-specific header

.apm_entry_loop:
    cmp ecx, ACPI_MADT_ENTRY_HEADER_SIZE ; Enough space for at least an entry header?
    jl .apm_parse_done ; If not, done parsing

    mov r8b, [rsi + ACPI_MADT_ENTRY_HEADER.Type]   ; R8B = Entry Type
    mov r9b, [rsi + ACPI_MADT_ENTRY_HEADER.Length] ; R9B = Entry Length

    cmp r9b, ACPI_MADT_ENTRY_HEADER_SIZE ; Entry length must be valid
    jl .apm_corrupt_madt ; Corrupt entry length

    ; Process entry based on type
    cmp r8b, ACPI_MADT_TYPE_LOCAL_APIC ; Type 0: Processor Local APIC
    je .apm_process_lapic
    cmp r8b, ACPI_MADT_TYPE_IO_APIC    ; Type 1: I/O APIC
    je .apm_process_ioapic
    cmp r8b, ACPI_MADT_TYPE_INTERRUPT_OVERRIDE ; Type 2: Interrupt Source Override
    je .apm_process_int_override
    ; Other types are ignored for now
    jmp .apm_next_entry

.apm_process_lapic:
    ; Expected length for Local APIC: Header (2) + ACPI_MADT_LOCAL_APIC (ProcID_1, ApicID_1, Flags_4 = 6) = 8
    cmp r9b, ACPI_MADT_ENTRY_HEADER_SIZE + 6 ; Size of ACPI_MADT_LOCAL_APIC struct is 6 bytes data
    jl .apm_next_entry ; Entry too short

    mov eax, [rsi + ACPI_MADT_ENTRY_HEADER_SIZE + ACPI_MADT_LOCAL_APIC.Flags]
    test eax, 1 ; Bit 0: Processor Enabled OR Bit 1: Online Capable
    jz .apm_lapic_not_enabled_or_capable ; if not (Enabled OR OnlineCapable), skip
    ; Also check bit 1 (Online Capable) if Enabled is not set?
    ; Standard is: (Flags & 1) || (Flags & 2)
    ; Current test eax, 1 only checks Enabled. Let's check both:
    ; test eax, 0x03 ; test bits 0 and 1
    ; jz .apm_lapic_not_enabled_or_capable
    ; For simplicity, if bit 0 (Enabled) is set, count it.
    ; Or, more inclusively: if (Enabled) or (Online Capable and Enabled via later mechanism)
    ; The rule usually is: if Flags & 1 (Enabled) is set OR Flags & 2 (Online Capable) is set
    ; This means: if ((Flags & 1) != 0) count++;
    ; A simpler check for now for "active" core:
    test eax, 1 ; Processor Enabled
    jz .apm_next_entry ; If not enabled, skip

    inc dword [gNumCpuCores_acpi]
    jmp .apm_next_entry

.apm_lapic_not_enabled_or_capable:
    jmp .apm_next_entry

.apm_process_ioapic:
    ; Expected length: Header (2) + ACPI_MADT_IO_APIC (ID_1,Res_1,Addr_4,GSIBase_4 = 10) = 12
    cmp r9b, ACPI_MADT_ENTRY_HEADER_SIZE + 10 ; Size of ACPI_MADT_IO_APIC struct is 10 bytes data
    jl .apm_next_entry ; Entry too short

    cmp qword [gIoApicAddress_acpi], 0 ; Only store the first I/O APIC found
    jne .apm_next_entry ; Already have one

    mov al, [rsi + ACPI_MADT_ENTRY_HEADER_SIZE + ACPI_MADT_IO_APIC.IoApicId]
    mov [gIoApicId_acpi], al
    mov eax, [rsi + ACPI_MADT_ENTRY_HEADER_SIZE + ACPI_MADT_IO_APIC.IoApicAddress]
    mov [gIoApicAddress_acpi], rax
    mov eax, [rsi + ACPI_MADT_ENTRY_HEADER_SIZE + ACPI_MADT_IO_APIC.GlobalSystemInterruptBase]
    mov [gIoApicBaseGsi_acpi], eax
    jmp .apm_next_entry

.apm_process_int_override:
    ; Expected length: Header (2) + ACPI_MADT_INT_OVERRIDE (Bus_1,Src_1,GSI_4,Flags_2 = 8) = 10
    cmp r9b, ACPI_MADT_ENTRY_HEADER_SIZE + 8 ; Size of ACPI_MADT_INT_OVERRIDE data part is 8 bytes
    jl .apm_next_entry

    mov dl, [gNumIrqOverrides_acpi]
    cmp dl, MAX_IRQ_OVERRIDES
    jge .apm_next_entry ; Override buffer full

    ; Calculate address in gIrqOverrides_acpi array
    movzx rdi, dl ; idx
    imul rdi, rdi, ACPI_MADT_INT_OVERRIDE_size ; offset = idx * struct_size
    lea rdi, [gIrqOverrides_acpi + rdi] ; RDI = &gIrqOverrides_acpi[idx]

    ; Copy data: RSI points to current MADT entry. Skip entry header to get to data.
    push rsi ; Save current MADT entry pointer
    add rsi, ACPI_MADT_ENTRY_HEADER_SIZE ; RSI now points to data part of override entry
    push rdi ; Save destination pointer for memcpy64
    ; RDI = dest, RSI = src, RCX = count for memcpy64
    mov ecx, ACPI_MADT_INT_OVERRIDE_size
    call memcpy64
    pop rdi ; Restore destination (not strictly needed here)
    pop rsi ; Restore current MADT entry pointer

    inc byte [gNumIrqOverrides_acpi]
    jmp .apm_next_entry

.apm_next_entry:
    add rsi, r9 ; Advance RSI by current entry's length
    sub ecx, r9d ; Subtract entry length from remaining length
    jmp .apm_entry_loop

.apm_corrupt_madt:
    ; Optionally log corruption
    ; For now, just stop parsing if an entry length is bad
.apm_parse_done:
    mov rax, EFI_SUCCESS
    jmp .apm_exit

.apm_not_found:
    mov rax, EFI_NOT_FOUND
    jmp .apm_exit_error_common

.apm_exit_error:
    ; CF is already set by acpi_find_table or error occurred here
    ; RAX might have an error code from acpi_find_table
    test rax, rax
    jnz .apm_exit ; Propagate error from acpi_find_table
.apm_exit_error_common:
    stc ; Ensure CF is set
    ; RAX might be 0 here if acpi_find_table returned 0 but CF was set.
    ; Set a generic error if RAX is 0.
    test rax, rax
    jnz .apm_exit
    mov rax, EFI_DEVICE_ERROR ; Generic error if specific one not set
.apm_exit:
    popaq
    ret

global acpi_parse_srat
acpi_parse_srat: ; Parses SRAT. Returns EFI_STATUS in RAX.
    pushaq
    mov dword [gNumaNodeCount_acpi], 0

    ; Find the SRAT table
    mov edi, ACPI_SIG_SRAT ; 'SRAT'
    call acpi_find_table
    jc .aps_exit_error      ; CF set if not found or error in acpi_find_table
    test rax, rax           ; RAX = SRAT address
    jz .aps_not_found       ; If address is 0, also not found

    ; SRAT found: RAX = SRAT address, RBX = SRAT length
    mov r12, rax ; R12 = SRAT physical address
    mov r13, rbx ; R13 = SRAT total length

    ; RSI = Pointer to the first SRAT entry
    mov rsi, r12
    add rsi, ACPI_TABLE_HEADER_SIZE ; Skip ACPI_TABLE_HEADER

    ; RCX = Remaining length of SRAT entries
    mov ecx, r13d                   ; Total SRAT length
    sub ecx, ACPI_TABLE_HEADER_SIZE ; Subtract ACPI_TABLE_HEADER

.aps_entry_loop:
    cmp ecx, ACPI_MADT_ENTRY_HEADER_SIZE ; Enough space for at least an entry header?
    jl .aps_parse_done ; If not, done parsing (ACPI_MADT_ENTRY_HEADER_SIZE is generic for type/length)

    mov r8b, [rsi + ACPI_MADT_ENTRY_HEADER.Type]   ; R8B = Entry Type
    mov r9b, [rsi + ACPI_MADT_ENTRY_HEADER.Length] ; R9B = CurrentEntryLength

    ; Validate Entry Length
    cmp r9b, ACPI_MADT_ENTRY_HEADER_SIZE
    jl .aps_corrupt_srat ; Entry length too small
    cmp r9b, cl ; Entry length bigger than remaining table size?
    jg .aps_corrupt_srat ; Yes, corrupt

    ; Process entry based on type
    cmp r8b, ACPI_SRAT_TYPE_MEMORY_AFFINITY ; Type 1: Memory Affinity
    je .aps_process_mem_affinity

    ; Other SRAT entry types are ignored for now
    jmp .aps_next_entry

.aps_process_mem_affinity:
    ; Expected total length for Memory Affinity entry is 40 bytes.
    ; (ACPI_MADT_ENTRY_HEADER_SIZE (2) + ACPI_SRAT_MEMORY_AFFINITY_data_size (38))
    cmp r9b, 40 ; Check if CurrentEntryLength is exactly 40
    jne .aps_next_entry ; Malformed or unexpected length for this type

    ; RDI_entry_data = RSI (current entry ptr) + ACPI_MADT_ENTRY_HEADER_SIZE
    mov rdi, rsi
    add rdi, ACPI_MADT_ENTRY_HEADER_SIZE ; RDI now points to start of ACPI_SRAT_MEMORY_AFFINITY data

    ; Check Flags (bit 0: Enabled)
    mov eax, [rdi + ACPI_SRAT_MEMORY_AFFINITY.Flags]
    test eax, SRAT_MEM_ENABLED ; Test bit 0 (SRAT_MEM_ENABLED should be 1)
    jz .aps_next_entry ; If not enabled, skip this memory region

    ; Check Length (LengthLow + LengthHigh)
    mov r14, [rdi + ACPI_SRAT_MEMORY_AFFINITY.LengthLow]  ; Low 32 bits of length
    mov r15, [rdi + ACPI_SRAT_MEMORY_AFFINITY.LengthHigh] ; High 32 bits of length
    shl r15, 32
    or r14, r15 ; R14 = 64-bit memory region length
    test r14, r14 ; If length is 0, skip
    jz .aps_next_entry

    ; Check if we have space to store this NUMA node info
    movzx r10d, dword [gNumaNodeCount_acpi] ; Current count of NUMA nodes found
    cmp r10d, MAX_NUMA_NODES
    jge .aps_next_entry ; Reached max storage for NUMA nodes

    ; Store info
    push rsi ; Save RSI (current SRAT entry pointer)
    mov rsi, r10d ; Use RSI as index for gNumaNodeInfo_acpi
    imul rsi, rsi, NUMA_NODE_MEM_INFO_acpi_size ; offset = index * struct_size
    lea rsi, [gNumaNodeInfo_acpi + rsi] ; RSI = &gNumaNodeInfo_acpi[index]

    ; BaseAddress (BaseAddressLow + BaseAddressHigh)
    mov rax, [rdi + ACPI_SRAT_MEMORY_AFFINITY.BaseAddressLow]
    mov rbx, [rdi + ACPI_SRAT_MEMORY_AFFINITY.BaseAddressHigh]
    shl rbx, 32
    or rax, rbx
    mov [rsi + NUMA_NODE_MEM_INFO_acpi_struc.BaseAddress], rax

    ; Length (already in R14)
    mov [rsi + NUMA_NODE_MEM_INFO_acpi_struc.Length], r14

    ; ProximityDomain
    mov eax, [rdi + ACPI_SRAT_MEMORY_AFFINITY.ProximityDomain]
    mov [rsi + NUMA_NODE_MEM_INFO_acpi_struc.ProximityDomain], eax

    ; Flags
    mov eax, [rdi + ACPI_SRAT_MEMORY_AFFINITY.Flags] ; Already read into EAX earlier for enabled check
    mov [rsi + NUMA_NODE_MEM_INFO_acpi_struc.Flags], eax

    inc dword [gNumaNodeCount_acpi]
    pop rsi ; Restore RSI (current SRAT entry pointer)
    jmp .aps_next_entry

.aps_next_entry:
    add rsi, r9 ; Advance RSI by CurrentEntryLength
    sub ecx, r9d ; Subtract CurrentEntryLength from remaining length
    jmp .aps_entry_loop

.aps_corrupt_srat:
    ; Optionally log corruption
.aps_parse_done:
    mov rax, EFI_SUCCESS
    jmp .aps_exit

.aps_not_found:
    mov rax, EFI_NOT_FOUND
    jmp .aps_exit_error_common

.aps_exit_error:
    ; CF is already set by acpi_find_table or error occurred here
    test rax, rax
    jnz .aps_exit ; Propagate error from acpi_find_table
.aps_exit_error_common:
    stc ; Ensure CF is set
    test rax, rax
    jnz .aps_exit
    mov rax, EFI_DEVICE_ERROR ; Generic error if specific one not set
.aps_exit:
    popaq
    ret

BITS 64
%include "boot_defs_temp.inc"

global simple_pmm64_init_uefi ; Renamed
global simple_pmm_alloc_frame, simple_pmm_free_frame, simple_pmm_mark_region_used ; Renamed

; Externs for addresses/data needed from other modules
extern gNumaNodeInfo_acpi, gNumaNodeCount_acpi ; From acpi.asm (for future NUMA integration)
extern _kernel_start, _kernel_end             ; Linker symbols for kernel boundaries
extern gop_framebuffer_base, gop_framebuffer_size ; From main_uefi_loader.asm
extern gFinalGdtBase, gFinalIdtBase           ; From main_uefi_loader.asm
extern gAcpiSdtAddress                        ; From acpi.asm (to reserve ACPI tables space)
extern gRsdpAddress                           ; From acpi.asm (to reserve RSDP space if found in conventional mem)

section .data align=8
    PMM_PAGE_SIZE equ PAGE_SIZE_4K ; Use 4KB pages for PMM
    pmm_initialized db 0

section .bss align=4096 ; Align bitmap start to a page boundary
    gPmmBitmapAddress dq 0
    gPmmBitmapSizeInBytes dq 0
    gPmmTotalPhysicalMemory dq 0 ; Highest physical address found + size of last block (effectively total span)
    gPmmTotalFrames dq 0
    gPmmUsedFrames dq 0
    gPmmHighestPhysicalAddressFound dq 0 ; Stores the highest end address of any memory block

; Helper function: pmm_set_bit (marks a frame as used)
; Input: RAX = frame_index
; Output: CF=1 if bit was already set, CF=0 if bit was clear and is now set.
; Clobbers: RDI, RCX, RBX (internal to this helper)
simple_pmm_set_bit: ; Renamed
    push rbx

    mov rdi, [gPmmBitmapAddress]
    mov rbx, rax
    shr rbx, 3           ; RBX = byte_index (RAX / 8)
    add rdi, rbx         ; RDI points to the byte in bitmap
    mov cl, al
    and cl, 7            ; CL = bit_index (RAX % 8)
    bt byte [rdi], cl    ; Test current state, result in CF
    bts byte [rdi], cl   ; Set the bit

    pop rbx
    ret

; Helper function: pmm_clear_bit (marks a frame as free)
; Input: RAX = frame_index
; Output: CF=1 if bit was already clear, CF=0 if bit was set and is now clear.
; Clobbers: RDI, RCX, RBX (internal to this helper)
simple_pmm_clear_bit: ; Renamed
    push rbx

    mov rdi, [gPmmBitmapAddress]
    mov rbx, rax
    shr rbx, 3           ; byte_index
    add rdi, rbx
    mov cl, al
    and cl, 7            ; bit_index
    bt byte [rdi], cl    ; Test current state, result in CF
    btr byte [rdi], cl   ; Clear the bit
    
    pop rbx
    ret

; Helper function: pmm_test_bit (checks if a frame is used)
; Input: RAX = frame_index
; Output: CF=1 if bit is set (used), CF=0 if bit is clear (free)
; Clobbers: RDI, RCX, RBX (internal to this helper)
simple_pmm_test_bit: ; Renamed
    push rbx

    mov rdi, [gPmmBitmapAddress]
    mov rbx, rax
    shr rbx, 3           ; byte_index
    add rdi, rbx
    mov cl, al
    and cl, 7            ; bit_index
    bt byte [rdi], cl    ; Test bit, result in CF
    
    pop rbx
    ret

section .text
simple_pmm64_init_uefi: ; Renamed - Input: RCX=UEFI MapPtr, RDX=UEFI MapSize, R8=UEFI DescSize
    pushaq
    mov byte [pmm_initialized], 0

    ; Preserve input arguments as they will be needed for multiple passes
    mov r12, rcx ; R12 = UEFI Map Ptr
    mov r13, rdx ; R13 = UEFI Map Size
    mov r14, r8  ; R14 = UEFI Desc Size

    ; --- 1. Find Max Physical Address from UEFI Memory Map ---
    mov r9, r12  ; R9 = Current UEFI Map Descriptor Pointer (from preserved R12)
    mov r10, r13 ; R10 = Remaining UEFI Map Size (from preserved R13)
    mov qword [gPmmHighestPhysicalAddressFound], 0

.find_max_addr_loop_pmm:
    cmp r10, r14 ; Enough size for one more descriptor? (R14=DescSize)
    jl .find_max_addr_done_pmm

    mov rax, [r9 + OFFSET_MD_PHYSICALSTART]
    mov rbx, [r9 + OFFSET_MD_NUMBEROFPAGES]
    shl rbx, 12 ; Convert NumberOfPages to bytes (PMM_PAGE_SIZE is 4096, shift 12)
    add rax, rbx ; RAX = End address of this memory block (exclusive)

    cmp rax, [gPmmHighestPhysicalAddressFound]
    jbe .find_max_addr_next_desc_pmm
    mov [gPmmHighestPhysicalAddressFound], rax

.find_max_addr_next_desc_pmm:
    add r9, r14  ; Move to next descriptor (R14=DescSize)
    sub r10, r14 ; Decrement remaining map size
    jmp .find_max_addr_loop_pmm
.find_max_addr_done_pmm:

    mov rax, [gPmmHighestPhysicalAddressFound]
    test rax, rax
    jz .pmm_init_fail_no_memory_pmm ; No physical memory found? Error.

    mov [gPmmTotalPhysicalMemory], rax ; This is actually the span, not total usable.
    xor rdx, rdx
    mov rcx, PMM_PAGE_SIZE
    div rcx      ; RAX = Total number of page frames based on span
    mov [gPmmTotalFrames], rax

    ; Calculate bitmap size in bytes: (TotalFrames + 7) / 8
    add rax, 7
    mov rbx, 8
    xor rdx, rdx
    div rbx      ; RAX = Bitmap size in bytes
    ; Align bitmap size up to a page boundary
    mov r15, rax ; R15 = raw bitmap size
    add r15, PMM_PAGE_SIZE - 1
    mov rbx, PMM_PAGE_SIZE
    xor rdx, rdx
    mov rax, r15
    div rbx
    mul rbx      ; RAX = page-aligned bitmap size
    mov [gPmmBitmapSizeInBytes], rax
    mov r15, rax ; R15 = page-aligned bitmap size in bytes

    ; --- 2. Find Space for Bitmap ---
    mov r9, r12  ; Restore R9 = UEFI Map Ptr
    mov r10, r13 ; Restore R10 = UEFI Map Size
.find_bitmap_space_loop_pmm:
    cmp r10, r14 ; R14 = DescSize
    jl .pmm_init_fail_no_bitmap_space_pmm

    mov edi, [r9 + OFFSET_MD_TYPE]
    cmp edi, EfiConventionalMemory
    jne .find_bitmap_space_next_desc_pmm

    mov rax, [r9 + OFFSET_MD_PHYSICALSTART] ; Physical start of conventional block
    mov rbx, [r9 + OFFSET_MD_NUMBEROFPAGES]
    shl rbx, 12 ; Size of this region in bytes

    ; Ensure start is page aligned (it should be from UEFI)
    ; Check if region is large enough for page-aligned bitmap (R15)
    cmp rbx, r15
    jl .find_bitmap_space_next_desc_pmm

    ; Found space. RAX = PhysicalStart of region for bitmap.
    mov [gPmmBitmapAddress], rax
    jmp .bitmap_space_found_pmm
.find_bitmap_space_next_desc_pmm:
    add r9, r14
    sub r10, r14
    jmp .find_bitmap_space_loop_pmm
.pmm_init_fail_no_bitmap_space_pmm:
    jmp .pmm_init_fail_pmm ; Error

.bitmap_space_found_pmm:
    ; --- 3. Initialize Bitmap (all frames used) ---
    mov rdi, [gPmmBitmapAddress]
    mov rcx, [gPmmBitmapSizeInBytes] ; Use the actual (potentially unaligned) byte size for memset
    mov al, 0xFF ; Set all bits to 1 (used)
    rep stosb
    mov rax, [gPmmTotalFrames]
    mov [gPmmUsedFrames], rax

    ; --- 4. Free Usable Memory Regions in Bitmap ---
    mov r9, r12  ; Restore R9 = UEFI Map Ptr
    mov r10, r13 ; Restore R10 = UEFI Map Size
.free_regions_loop_pmm:
    cmp r10, r14 ; R14 = DescSize
    jl .free_regions_done_pmm

    mov edi, [r9 + OFFSET_MD_TYPE]
    cmp edi, EfiConventionalMemory
    jne .free_regions_next_desc_pmm

    mov rbx, [r9 + OFFSET_MD_PHYSICALSTART] ; RBX = Start of conventional memory block
    mov rcx, [r9 + OFFSET_MD_NUMBEROFPAGES] ; RCX = Number of pages in this block

.free_pages_in_block_loop_pmm:
    cmp rcx, 0
    jz .free_regions_next_desc_pmm

    ; RBX = current physical page address to free
    cmp rbx, [gPmmTotalPhysicalMemory] ; Prevent processing beyond managed range
    jge .skip_this_page_free_pmm

    mov rax, rbx ; Current physical page address
    xor rdx, rdx
    mov r15, PMM_PAGE_SIZE ; Divisor
    div r15 ; RAX = frame_index for current physical page rbx

    ; Corrected logic for freeing:
    push rax ; Save calculated frame_index
    call simple_pmm_test_bit ; Test current state. RAX is frame_index. CF=1 if used.
    pop rax ; Restore frame_index
    jnc .page_already_free_pmm ; If CF=0 (was free), skip.

    call simple_pmm_clear_bit ; Clears the bit. (RAX is frame_index)
    dec qword [gPmmUsedFrames]

.page_already_free_pmm:
.skip_this_page_free_pmm:
    add rbx, PMM_PAGE_SIZE ; Next page address in this block
    dec rcx ; Decrement page counter for this block
    jmp .free_pages_in_block_loop_pmm

.free_regions_next_desc_pmm:
    add r9, r14 ; R14 = DescSize
    sub r10, r14
    jmp .free_regions_loop_pmm
.free_regions_done_pmm:

    ; --- 5. Mark Reserved Regions as Used (Kernel, Bitmap, etc.) ---
    ; This is done by pmm_mark_region_used, called by main_uefi_loader
    ; However, the bitmap itself must be marked used here.
    mov rbx, [gPmmBitmapAddress]
    mov rcx, [gPmmBitmapAddress]
    add rcx, [gPmmBitmapSizeInBytes] ; End of bitmap region
.mark_bitmap_loop_pmm:
    cmp rbx, rcx
    jge .mark_bitmap_done_pmm
    mov rax, rbx
    xor rdx, rdx
    mov r15, PMM_PAGE_SIZE
    div r15 ; RAX = frame_index

    ; Corrected logic for marking bitmap used:
    push rax ; Save frame_index
    call simple_pmm_test_bit ; Test current state. RAX is frame_index. CF=1 if used.
    pop rax ; Restore frame_index
    jc .bitmap_page_already_used_pmm ; If already set, skip incrementing used count

    call simple_pmm_set_bit ; (RAX still frame_index)
    inc qword [gPmmUsedFrames]
.bitmap_page_already_used_pmm:
    add rbx, PMM_PAGE_SIZE
    jmp .mark_bitmap_loop_pmm
.mark_bitmap_done_pmm:

    mov byte [pmm_initialized], 1
    mov rax, EFI_SUCCESS
    jmp .pmm_init_exit_pmm

.pmm_init_fail_no_memory_pmm:
.pmm_init_fail_pmm:
    mov rax, EFI_OUT_OF_RESOURCES
.pmm_init_exit_pmm:
    popaq
    ret

; Marks a region of memory as used in the PMM bitmap
; Input: RAX = physical base address, RDX = size in bytes
; Output: Modifies PMM bitmap and gPmmUsedFrames
simple_pmm_mark_region_used: ; Renamed
    pushaq
    mov r8, rax  ; R8 = current address in region
    mov r9, rax  ; R9 = base address
    add r9, rdx  ; R9 = end address of region (exclusive)

    ; Align start address R8 down to page boundary (already should be for most cases)
    mov r10, PMM_PAGE_SIZE -1
    not r10
    and r8, r10

.mark_loop_pmru:
    cmp r8, r9 ; current_addr >= end_addr?
    jge .mark_done_pmru

    mov rax, r8 ; current page frame address
    xor rdx, rdx
    mov rcx, PMM_PAGE_SIZE
    div rcx      ; RAX = frame_index

    ; Corrected logic for marking region used
    push rax ; Save frame_index
    call simple_pmm_test_bit ; Test current state. RAX is frame_index. CF=1 if used.
    pop rax ; Restore frame_index
    jc .page_already_marked_used_pmru ; If already set, skip incrementing used count

    call simple_pmm_set_bit ; Mark as used (RAX = frame_index)
    inc qword [gPmmUsedFrames]
.page_already_marked_used_pmru:
    add r8, PMM_PAGE_SIZE
    jmp .mark_loop_pmru
.mark_done_pmru:
    popaq
    ret

; Placeholder/Simple PMM Alloc/Free (to be expanded later)
simple_pmm_alloc_frame: ; Renamed - Output: RAX = phys_addr, or 0 if error/no memory
    ; TODO: Implement actual search for free bit in bitmap, set bit, inc used_frames
    ; For now, just a stub
    test byte [pmm_initialized], 1
    jz .paf_not_init

    ; Basic linear search for a free frame (inefficient)
    mov rcx, [gPmmTotalFrames]
    mov rsi, 0 ; frame_index
.paf_search_loop:
    cmp rsi, rcx
    jge .paf_no_memory ; Reached end of bitmap
    mov rax, rsi
    call simple_pmm_test_bit ; Renamed - CF=1 if used, CF=0 if free
    jnc .paf_found_free ; If CF=0 (free), use this frame

    inc rsi
    jmp .paf_search_loop

.paf_found_free:
    mov rax, rsi
    call simple_pmm_set_bit ; Renamed - Mark as used
    inc qword [gPmmUsedFrames]
    
    mov rax, rsi ; frame_index
    mov rbx, PMM_PAGE_SIZE
    mul rbx      ; RAX = physical_address
    jmp .paf_exit

.paf_no_memory:
    xor rax, rax ; Return 0 for no memory
.paf_not_init:
    xor rax, rax ; Not initialized, return 0
.paf_exit:
    ret

simple_pmm_free_frame: ; Renamed - Input: RAX = phys_addr to free
    test byte [pmm_initialized], 1
    jz .pff_exit ; Not initialized

    ; Convert phys_addr to frame_index
    xor rdx, rdx
    mov rcx, PMM_PAGE_SIZE
    div rcx ; RAX = frame_index

    cmp rax, [gPmmTotalFrames]
    jge .pff_exit ; Invalid frame index

    call simple_pmm_test_bit ; Renamed - CF=1 if used, CF=0 if free
    jnc .pff_already_free ; If already free, do nothing or error

    call simple_pmm_clear_bit ; Renamed - Mark as free
    dec qword [gPmmUsedFrames]
.pff_already_free:
.pff_exit:
    ret

; pmm64_uefi.asm: UEFI-centric 64-bit Physical Memory Manager (Optimized)
; Depends on: boot_defs_temp.inc (with UEFI defs)
; Depends on: UefiAllocatePages (from main_uefi_loader.asm / uefi_wrappers.asm)

BITS 64
%include "boot_defs_temp.inc"

global pmm64_init_uefi
global pmm_alloc_frame
global pmm_free_frame
global pmm_alloc_frame_node
global pmm_free_frame_node
global pmm_alloc_large_frame
global pmm_free_large_frame
global pmm_mark_region_used
global pmm_mark_region_free
global pmm_total_frames
global pmm_used_frames
global pmm_max_ram_addr
global pmm_total_large_frames
global pmm_used_large_frames
global numa_node_count ; Defined in main_uefi_loader.asm

extern UefiAllocatePages ; From main_uefi_loader.asm (wrappers section)
extern efi_image_base
extern efi_image_size
extern gFinalGdtBase     ; From main_uefi_loader.asm
extern gFinalIdtBase     ; From main_uefi_loader.asm
extern gop_framebuffer_base
extern gop_framebuffer_size

%ifndef SMP
    %define SMP 0
%endif
%if SMP
    %define LOCK_PREFIX lock
%else
    %define LOCK_PREFIX
%endif

section .data align=64
pmm_total_frames dq 0
pmm_used_frames dq 0
pmm_total_large_frames dq 0
pmm_used_large_frames dq 0
pmm_max_ram_addr dq 0
pmm_node_base_addrs dq 0,0,0,0,0,0,0,0
pmm_node_addr_limits dq 0,0,0,0,0,0,0,0
pmm_node_bitmaps dq 0,0,0,0,0,0,0,0
pmm_node_large_bitmaps dq 0,0,0,0,0,0,0,0
pmm_node_frame_counts dq 0,0,0,0,0,0,0,0
pmm_node_large_frame_counts dq 0,0,0,0,0,0,0,0
last_alloc_index_4k dq 0,0,0,0,0,0,0,0
last_alloc_index_large dq 0,0,0,0,0,0,0,0
page_size_4k_minus_1 dq PAGE_SIZE_4K - 1
page_size_2m_minus_1 dq PAGE_SIZE_2M - 1
temp_phys_addr_pmm dq 0 ; Renamed to avoid conflict

PMM_INIT_ERR_OK         equ 0
PMM_INIT_ERR_NO_MEM     equ 1
PMM_INIT_ERR_ALLOC_FAIL equ 2
PMM_INIT_ERR_NUMA_CFG   equ 3 ; Placeholder for NUMA config issues

section .text

;-----------------------------------------------------------------------------
pmm_get_node_for_addr: ; Input RDI=PhysAddr -> RAX=NodeID
    movzx rcx, byte [numa_node_count]
    cmp rcx, 1
    jle .pgnfa_force_node_0

    xor rax, rax
.pgnfa_node_check_loop:
    cmp rax, rcx
    jae .pgnfa_force_node_0

    mov rdx, [pmm_node_base_addrs + rax*8]
    cmp rdi, rdx
    jl .pgnfa_next_node

    mov rdx, [pmm_node_addr_limits + rax*8]
    cmp rdi, rdx
    jge .pgnfa_next_node

    ret ; RAX = Node ID
.pgnfa_next_node:
    inc rax
    jmp .pgnfa_node_check_loop
.pgnfa_force_node_0:
    xor rax, rax
    ret

;-----------------------------------------------------------------------------
zero_physical_pages_pmm: ; Input RDI=StartAddr, RCX=NumPages
    test rcx, rcx
    jz .zpp_done
    mov rdx, rcx
    shl rdx, 12 ; bytes_count
    add rdx, rdi ; end_addr_exclusive
.zpp_loop:
    cmp rdi, rdx
    jae .zpp_done
    push rcx
    push rdi
    xor eax, eax
    mov rcx, PAGE_SIZE_4K / 8
    rep stosq
    pop rdi
    add rdi, PAGE_SIZE_4K
    pop rcx
    jmp .zpp_loop
.zpp_done:
    ret

;-----------------------------------------------------------------------------
calculate_bitmap_size_pages: ; Input RDI=num_frames -> RAX=pages_needed
    mov rax, rdi
    add rax, 7
    shr rax, 3 ; bytes_needed
    add rax, PAGE_SIZE_4K - 1
    shr rax, 12 ; pages_needed
    ret

;-----------------------------------------------------------------------------
mark_bitmap_region: ; RDI=BitmapBase,RAX=StartIdx,RDX=EndIdx,R8=TotalFramesInNode,R9=Val(0/1)
    xor rcx, rcx ; Min index = 0
    cmp rax, rcx
    cmovl rax, rcx ; Start = max(Start, 0)
    cmp rdx, r8
    cmovg rdx, r8  ; End = min(End, TotalFrames)
    cmp rax, rdx
    jae .mbr_done  ; Start >= End, nothing to do

    mov rbx, rax  ; RBX = start_frame_index
    shr rbx, 6    ; RBX = start_qword_index
    mov sil, al   ; SIL = start_bit_offset (0-63)
    and sil, 63

    mov r10, rdx  ; R10 = end_frame_index (exclusive)
    shr r10, 6    ; R10 = end_qword_index
    mov r8b, dl   ; R8B = end_bit_offset (0-63) for end_qword
    and r8b, 63

    lea r11, [rdi + rbx*8] ; R11 = pointer to bitmap[start_qword_index]

    test r9, r9 ; Test if marking used (R9=1) or free (R9=0)
    jnz .mbr_mark_used_path

; --- Mark FREE (R9=0 -> AND with inverted mask) ---
.mbr_mark_free_path:
    cmp rbx, r10 ; Is start_qword == end_qword (single qword case)?
    jne .mbr_free_multi_qword

    ; Single QWORD case
    mov rax, -1      ; All ones
    mov rcx, -1      ; All ones
    shl rax, sil     ; Create mask for bits >= start_bit_offset
    test r8b, r8b    ; Is end_bit_offset zero? (means up to end of qword)
    jz .mbr_free_single_no_end_mask
    shr rcx, (64 - r8b) ; Create mask for bits < end_bit_offset
    and rax, rcx     ; Combined mask for bits [start_bit, end_bit)
.mbr_free_single_no_end_mask:
    not rax          ; Invert mask to clear target bits
    LOCK_PREFIX and [r11], rax
    jmp .mbr_done

.mbr_free_multi_qword:
    ; 1. Handle first partial QWORD
    mov rax, -1
    shl rax, sil     ; Mask for bits >= start_bit_offset
    not rax          ; Invert mask to clear target bits
    LOCK_PREFIX and [r11], rax
    add r11, 8       ; Move to next qword address
    inc rbx          ; Move to next qword index

    ; 2. Handle full middle QWORDs
    mov rax, 0       ; Value to clear bits (all zeros)
.mbr_free_middle_loop:
    cmp rbx, r10
    jae .mbr_free_last_qword
    mov [r11], rax   ; No LOCK needed if PMM init is single-threaded, else LOCK_PREFIX mov
    add r11, 8
    inc rbx
    jmp .mbr_free_middle_loop

.mbr_free_last_qword:
    ; 3. Handle last partial QWORD (if end_bit_offset > 0)
    test r8b, r8b
    jz .mbr_done    ; No bits to clear in the last qword if offset is 0
    mov rax, -1
    shr rax, (64 - r8b) ; Create mask for bits < end_bit_offset
    not rax          ; Invert mask to clear target bits
    LOCK_PREFIX and [r11], rax
    jmp .mbr_done

; --- Mark USED (R9=1 -> OR with mask) ---
.mbr_mark_used_path:
    cmp rbx, r10 ; Is start_qword == end_qword (single qword case)?
    jne .mbr_used_multi_qword

    ; Single QWORD case
    mov rax, -1
    mov rcx, -1
    shl rax, sil
    test r8b, r8b
    jz .mbr_used_single_no_end_mask
    shr rcx, (64 - r8b)
    and rax, rcx
.mbr_used_single_no_end_mask:
    LOCK_PREFIX or [r11], rax
    jmp .mbr_done

.mbr_used_multi_qword:
    ; 1. Handle first partial QWORD
    mov rax, -1
    shl rax, sil
    LOCK_PREFIX or [r11], rax
    add r11, 8
    inc rbx

    ; 2. Handle full middle QWORDs
    mov rax, -1 ; Value to set bits (all ones)
.mbr_used_middle_loop:
    cmp rbx, r10
    jae .mbr_used_last_qword
    mov [r11], rax
    add r11, 8
    inc rbx
    jmp .mbr_used_middle_loop

.mbr_used_last_qword:
    ; 3. Handle last partial QWORD (if end_bit_offset > 0)
    test r8b, r8b
    jz .mbr_done
    mov rax, -1
    shr rax, (64 - r8b)
    LOCK_PREFIX or [r11], rax
    ; Fall through to done
.mbr_done:
    ret

;-----------------------------------------------------------------------------
pmm64_init_uefi: ; Input: RCX=MapPtr, RDX=MapSize, R8=DescSize
    push rbp
    mov rbp, rsp
    sub rsp, 8 ; Align stack for potential calls, ensure 16-byte alignment before further calls
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; --- Phase 0: Populate PMM NUMA node info from ACPI SRAT data ---
    mov ecx, [gNumaNodeCount_acpi] ; Get count from ACPI parsing (dword)
    test ecx, ecx
    jnz .got_node_count_pmm_init
    mov ecx, 1 ; Default to 1 node if ACPI found none
.got_node_count_pmm_init:
    ; MAX_NUMA_NODES in this PMM is implicitly 8 due to array sizes like pmm_node_base_addrs
    cmp ecx, 8 ; Compare with max nodes supported by PMM's internal arrays
    jbe .count_ok_pmm_init
    mov ecx, 8 ; Clamp to max supported by PMM arrays
.count_ok_pmm_init:
    mov [numa_node_count], cl ; Store the effective node count for PMM (byte)

    xor r9, r9 ; Loop counter i = 0
.populate_node_loop_pmm_init:
    movzx r10d, byte [numa_node_count] ; Load effective node count
    cmp r9b, r10b ; Compare i with effective_node_count
    jge .populate_node_done_pmm_init

    ; RSI_src = &gNumaNodeInfo_acpi[i * NUMA_NODE_MEM_INFO_acpi_size]
    mov rsi, r9
    mov r10, NUMA_NODE_MEM_INFO_acpi_size ; NUMA_NODE_MEM_INFO_acpi_size = 24
    mul r10  ; RAX = r9 * 24 (lower part of rsi * 24)
    mov rsi, rax
    lea rsi, [gNumaNodeInfo_acpi + rsi]

    mov rax, [rsi + NUMA_NODE_MEM_INFO_acpi_struc.BaseAddress]
    mov [pmm_node_base_addrs + r9*8], rax
    mov rdx, [rsi + NUMA_NODE_MEM_INFO_acpi_struc.Length]
    add rax, rdx ; End address = Base + Length
    mov [pmm_node_addr_limits + r9*8], rax

    inc r9b
    jmp .populate_node_loop_pmm_init
.populate_node_done_pmm_init:

    mov r12, rcx ; UEFI Map Buffer Ptr (original RCX from arguments)
    mov r13, rdx ; UEFI Map Size (original RDX)
    mov r14, r8  ; UEFI Descriptor Size (original R8)

    ; --- Phase 1: Calculate Memory Size and Per-Node Frame Counts ---
    mov r15, 0   ; Temp for pmm_max_ram_addr
    mov qword [pmm_total_frames], 0
    mov qword [pmm_total_large_frames], 0

    movzx rcx, byte [numa_node_count] ; Zero extend node count
    cmp rcx, 1
    jne .numa_info_from_acpi ; If >1 node, assume pmm_node_base/limits pre-populated
    mov qword [pmm_node_base_addrs], 0 ; Node 0 starts at 0
    ; pmm_node_addr_limits[0] will be set after map scan from pmm_max_ram_addr
.numa_info_from_acpi:

    lea rdi, [pmm_node_frame_counts]
    xor eax, eax
    push rcx ; Save node count for rep stosq
    rep stosq ; Zero pmm_node_frame_counts (uses rcx from above as count)
    pop rcx   ; Restore node count
    lea rdi, [pmm_node_large_frame_counts]
    push rcx
    rep stosq ; Zero pmm_node_large_frame_counts
    pop rcx

    mov rbx, r12 ; RBX = Current Descriptor Pointer
    mov r9, r13  ; R9 = Remaining map size
.pi_calc_loop:
    cmp r9, r14 ; Check if enough bytes remain for one descriptor
    jl .pi_calc_done

    mov edi, [rbx + EFI_MEMORY_DESCRIPTOR.Type]
    mov rsi, [rbx + EFI_MEMORY_DESCRIPTOR.PhysicalStart]
    mov r10, [rbx + EFI_MEMORY_DESCRIPTOR.NumberOfPages]

    mov rax, r10
    shl rax, 12 ; Size in bytes
    add rax, rsi ; End address
    cmp rax, r15
    cmova r15, rax ; r15 tracks max address seen

    cmp edi, EfiConventionalMemory
    jne .pi_next_calc_desc

    add [pmm_total_frames], r10
    push rsi ; Save PhysicalStart
    push r10 ; Save NumberOfPages
    mov rdi, rsi ; Pass address to get_node
    call pmm_get_node_for_addr ; RAX = Node ID
    pop r10 ; Restore NumberOfPages
    pop rsi ; Restore PhysicalStart
    add [pmm_node_frame_counts + rax*8], r10

.pi_next_calc_desc:
    add rbx, r14 ; Move to next descriptor entry
    sub r9, r14  ; Decrement remaining map size
    jmp .pi_calc_loop
.pi_calc_done:
    mov [pmm_max_ram_addr], r15
    movzx rcx, byte [numa_node_count]
    cmp rcx, 1
    jne .pi_skip_node0_limit_set
    mov [pmm_node_addr_limits], r15 ; Set Node 0 limit if single node mode
.pi_skip_node0_limit_set:

    cmp qword [pmm_total_frames], 0
    je .pi_fail_no_mem

    movzx rcx, byte [numa_node_count]
    xor rbx, rbx ; Node index
.pi_calc_large_loop:
    cmp rbx, rcx
    jae .pi_calc_large_done
    mov rax, [pmm_node_frame_counts + rbx*8]
    mov rdx, rax
    shr rdx, 9 ; large_frames = frames / 512
    mov [pmm_node_large_frame_counts + rbx*8], rdx
    add [pmm_total_large_frames], rdx
    inc rbx
    jmp .pi_calc_large_loop
.pi_calc_large_done:

    ; --- Phase 2: Allocate Bitmaps using UEFI ---
    movzx rcx, byte [numa_node_count]
    xor rbx, rbx ; Node index
.pi_alloc_bitmap_loop:
    cmp rbx, rcx
    jae .pi_alloc_bitmaps_done

    ; Allocate 4KB bitmap for node RBX
    mov rdi, [pmm_node_frame_counts + rbx*8]
    test rdi, rdi
    jz .pi_skip_4k_alloc
    call calculate_bitmap_size_pages ; RDI=frame_count -> RAX=pages_needed
    mov r8, rax ; R8 = pages needed for 4k bitmap
    test r8, r8
    jz .pi_skip_4k_alloc

    push rbx ; Save node index
    push rcx ; Save node count for loops
    ; Call UefiAllocatePages: RCX=Type, RDX=MemType, R8=Pages, R9=AddrPtr
    mov rcx, AllocateAnyPages
    mov rdx, EfiLoaderData
    ; R8 = Pages (already set)
    lea r9, [temp_phys_addr_pmm]
    call UefiAllocatePages ; Wrapper from main_uefi_loader.asm
    pop rcx ; Restore node count
    pop rbx ; Restore node index
    test rax, rax ; Check Status from UefiAllocatePages
    jnz .pi_fail_alloc

    mov rdi, [temp_phys_addr_pmm]
    mov [pmm_node_bitmaps + rbx*8], rdi ; Store bitmap pointer
    push rbx ; Save node index
    push rcx ; Save node count
    mov rcx, r8 ; Number of pages to zero
    call zero_physical_pages_pmm ; Zero the allocated bitmap
    pop rcx
    pop rbx
.pi_skip_4k_alloc:

    ; Allocate 2MB (large) bitmap for node RBX
    mov rdi, [pmm_node_large_frame_counts + rbx*8]
    test rdi, rdi
    jz .pi_skip_large_alloc
    call calculate_bitmap_size_pages
    mov r8, rax ; R8 = pages needed for large bitmap
    test r8, r8
    jz .pi_skip_large_alloc

    push rbx; push rcx
    mov rcx, AllocateAnyPages; mov rdx, EfiLoaderData; lea r9, [temp_phys_addr_pmm]
    call UefiAllocatePages
    pop rcx; pop rbx
    test rax, rax; jnz .pi_fail_alloc

    mov rdi, [temp_phys_addr_pmm]
    mov [pmm_node_large_bitmaps + rbx*8], rdi
    push rbx; push rcx
    mov rcx, r8 ; Number of pages to zero
    call zero_physical_pages_pmm
    pop rcx; pop rbx
.pi_skip_large_alloc:
    inc rbx
    jmp .pi_alloc_bitmap_loop
.pi_alloc_bitmaps_done:

    ; --- Phase 3: Initialize Bitmaps (Mark all USED = set bits to 1) ---
    movzx rcx, byte [numa_node_count]
    xor rbx, rbx ; Node index
.pi_init_bitmap_loop:
    cmp rbx, rcx
    jae .pi_init_bitmaps_done

    mov rdi, [pmm_node_bitmaps + rbx*8]
    test rdi, rdi
    jz .pi_next_init_bitmap_part
    mov rsi, [pmm_node_frame_counts + rbx*8]
    add rsi, 63 ; Round up frame count to qwords
    shr rsi, 6  ; RSI = number of qwords in bitmap
    test rsi, rsi
    jz .pi_next_init_bitmap_part
    mov rax, -1 ; Value to store (all ones)
    push rcx; push rbx
    mov rcx, rsi ; QWORD count for rep stosq
    rep stosq   ; Fill bitmap with 1s
    pop rbx; pop rcx
.pi_next_init_bitmap_part:
    mov rdi, [pmm_node_large_bitmaps + rbx*8]
    test rdi, rdi
    jz .pi_next_init_node
    mov rsi, [pmm_node_large_frame_counts + rbx*8]
    add rsi, 63
    shr rsi, 6
    test rsi, rsi
    jz .pi_next_init_node
    mov rax, -1
    push rcx; push rbx
    mov rcx, rsi
    rep stosq
    pop rbx; pop rcx
.pi_next_init_node:
    inc rbx
    jmp .pi_init_bitmap_loop
.pi_init_bitmaps_done:

    ; --- Phase 4: Free Conventional Memory Regions ---
    mov rbx, r12 ; RBX = Current Descriptor Pointer (reset to start of map)
    mov r9, r13  ; R9 = Remaining map size (reset)
.pi_free_conv_loop:
    cmp r9, r14 ; Check if enough bytes remain
    jl .pi_free_conv_done

    mov edi, [rbx + EFI_MEMORY_DESCRIPTOR.Type]
    cmp edi, EfiConventionalMemory
    jne .pi_next_free_desc

    mov rsi, [rbx + EFI_MEMORY_DESCRIPTOR.PhysicalStart]
    mov r10, [rbx + EFI_MEMORY_DESCRIPTOR.NumberOfPages]
    mov rax, rsi ; Start address
    mov rdx, r10
    shl rdx, 12  ; Size in bytes
    add rdx, rax ; End address (exclusive)
    call pmm_mark_region_free

.pi_next_free_desc:
    add rbx, r14
    sub r9, r14
    jmp .pi_free_conv_loop
.pi_free_conv_done:

    ; --- Phase 5: Mark Bootloader/Kernel/Bitmap/GDT/IDT/Framebuffer Regions USED ---
    mov rax, [efi_image_base]
    mov rdx, [efi_image_size]
    add rdx, rax
    call pmm_mark_region_used

    movzx rcx, byte [numa_node_count]
    xor rbx, rbx ; Node index
.pi_mark_bitmap_loop:
    cmp rbx, rcx
    jae .pi_mark_bitmaps_done

    mov rax, [pmm_node_bitmaps + rbx*8]
    test rax, rax
    jz .pi_next_mark_bitmap_part
    mov rdi, [pmm_node_frame_counts + rbx*8]
    call calculate_bitmap_size_pages
    mov rdx, rax ; RDX = pages
    shl rdx, 12  ; RDX = size in bytes
    add rdx, rax ; End address
    call pmm_mark_region_used
.pi_next_mark_bitmap_part:
    mov rax, [pmm_node_large_bitmaps + rbx*8]
    test rax, rax
    jz .pi_next_mark_node
    mov rdi, [pmm_node_large_frame_counts + rbx*8]
    call calculate_bitmap_size_pages
    mov rdx, rax
    shl rdx, 12
    add rdx, rax
    call pmm_mark_region_used
.pi_next_mark_node:
    inc rbx
    jmp .pi_mark_bitmap_loop
.pi_mark_bitmaps_done:

    mov rax, [gFinalGdtBase]
    test rax,rax; jz .pi_skip_gdt_mark
    mov rdx, PAGE_SIZE_4K; add rdx, rax; call pmm_mark_region_used
.pi_skip_gdt_mark:
    mov rax, [gFinalIdtBase]
    test rax,rax; jz .pi_skip_idt_mark
    mov rdx, PAGE_SIZE_4K; add rdx, rax; call pmm_mark_region_used
.pi_skip_idt_mark:
    mov rax, [gop_framebuffer_base]
    test rax,rax; jz .pi_skip_fb_mark
    mov rdx, [gop_framebuffer_size]; add rdx, rax; call pmm_mark_region_used
.pi_skip_fb_mark:

    ; --- Finish ---
    mov rax, PMM_INIT_ERR_OK ; Success
    jmp .pi_exit

.pi_fail_no_mem:
    mov rax, PMM_INIT_ERR_NO_MEM
    jmp .pi_exit_final
.pi_fail_alloc:
    mov rax, PMM_INIT_ERR_ALLOC_FAIL
.pi_exit_final:
    ; No need to pop r15 etc here if jumping out
    ; The caller's frame will be restored by its ret.
    ; Just ensure stack is balanced from this function's perspective.
    add rsp, 8 ; Balance stack from sub rsp, 8
    pop r15; pop r14; pop r13; pop r12; pop rbx
    pop rbp
    ret

.pi_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 8 ; Balance stack from sub rsp, 8
    pop rbp
    ret

; --- Core Allocation/Freeing/Marking Functions ---
; (Implementations from previous response, ensure unique labels if conflicts)
pmm_mark_region_used:
    push rbx;push rcx;push rdx;push rsi;push rdi;push r8;push r9;push r10;push r11;push r12;push r13;push r14;push r15
    mov r14, rax; mov r15, rdx; mov rax, r14; mov rdx, r15; mov r8, rax; and r8, [page_size_4k_minus_1]; jz .s4k_a_mru_impl; and rax, 0xFFFFFFFFFFFFF000; .s4k_a_mru_impl: mov rsi, rdx; and rsi, [page_size_4k_minus_1]; jz .e4k_a_mru_impl; and rdx, 0xFFFFFFFFFFFFF000; add rdx, PAGE_SIZE_4K; .e4k_a_mru_impl: cmp rax, rdx; jae .mark_large_mru_impl; mov r12, rax; mov r13, rdx
    movzx rcx, byte [numa_node_count]; xor rbx, rbx
.node_4k_loop_mru_impl: cmp rbx, rcx; jae .mark_large_mru_impl; mov rdi, [pmm_node_bitmaps + rbx*8]; test rdi, rdi; jz .next_node_4k_mru_impl; mov r8, [pmm_node_frame_counts + rbx*8]; test r8, r8; jz .next_node_4k_mru_impl; mov r10, [pmm_node_base_addrs + rbx*8]; mov r11, [pmm_node_addr_limits + rbx*8]; mov rax, r12; cmp rax, r10; cmovb rax, r10; mov rdx, r13; cmp rdx, r11; cmova rdx, r11; cmp rax, rdx; jae .next_node_4k_mru_impl; sub rax, r10; sub rdx, r10; shr rax, 12; shr rdx, 12; mov r9, 1; call mark_bitmap_region
.next_node_4k_mru_impl: inc rbx; jmp .node_4k_loop_mru_impl
.mark_large_mru_impl: mov rax, r14; mov rdx, r15; mov r8, rax; and r8, [page_size_2m_minus_1]; jz .s2m_a_mru_impl; mov r10, [page_size_2m_minus_1]; not r10; and rax, r10; .s2m_a_mru_impl: mov rsi, rdx; and rsi, [page_size_2m_minus_1]; jz .e2m_a_mru_impl; mov r10, [page_size_2m_minus_1]; not r10; and rdx, r10; add rdx, PAGE_SIZE_2M; .e2m_a_mru_impl: cmp rax, rdx; jae .done_mark_used_mru_impl
    mov r12, rax; mov r13, rdx; movzx rcx, byte [numa_node_count]; xor rbx, rbx
.node_2m_loop_mru_impl: cmp rbx, rcx; jae .done_mark_used_mru_impl; mov rdi, [pmm_node_large_bitmaps + rbx*8]; test rdi, rdi; jz .next_node_2m_mru_impl; mov r8, [pmm_node_large_frame_counts + rbx*8]; test r8, r8; jz .next_node_2m_mru_impl; mov r10, [pmm_node_base_addrs + rbx*8]; mov r11, [pmm_node_addr_limits + rbx*8]; mov rax, r12; cmp rax, r10; cmovb rax, r10; mov rdx, r13; cmp rdx, r11; cmova rdx, r11; cmp rax, rdx; jae .next_node_2m_mru_impl; sub rax, r10; sub rdx, r10; shr rax, 21; shr rdx, 21; mov r9, 1; call mark_bitmap_region
.next_node_2m_mru_impl: inc rbx; jmp .node_2m_loop_mru_impl
.done_mark_used_mru_impl: pop r15;pop r14;pop r13;pop r12;pop r11;pop r10;pop r9;pop r8;pop rdi;pop rsi;pop rdx;pop rcx;pop rbx; ret

pmm_mark_region_free:
    push rbx;push rcx;push rdx;push rsi;push rdi;push r8;push r9;push r10;push r11;push r12;push r13;push r14;push r15
    mov r14, rax; mov r15, rdx; mov rax, r14; mov rdx, r15; mov r8, rax; and r8, [page_size_4k_minus_1]; jz .s4k_a_mrf_impl; and rax, 0xFFFFFFFFFFFFF000; add rax, PAGE_SIZE_4K; .s4k_a_mrf_impl: mov rsi, rdx; and rsi, [page_size_4k_minus_1]; jz .e4k_a_mrf_impl; and rdx, 0xFFFFFFFFFFFFF000; .e4k_a_mrf_impl: cmp rax, rdx; jae .mark_large_mrf_impl; mov r12, rax; mov r13, rdx
    movzx rcx, byte [numa_node_count]; xor rbx, rbx
.node_4k_loop_mrf_impl: cmp rbx, rcx; jae .mark_large_mrf_impl; mov rdi, [pmm_node_bitmaps + rbx*8]; test rdi, rdi; jz .next_node_4k_mrf_impl; mov r8, [pmm_node_frame_counts + rbx*8]; test r8, r8; jz .next_node_4k_mrf_impl; mov r10, [pmm_node_base_addrs + rbx*8]; mov r11, [pmm_node_addr_limits + rbx*8]; mov rax, r12; cmp rax, r10; cmovb rax, r10; mov rdx, r13; cmp rdx, r11; cmova rdx, r11; cmp rax, rdx; jae .next_node_4k_mrf_impl; sub rax, r10; sub rdx, r10; shr rax, 12; shr rdx, 12; mov r9, 0; call mark_bitmap_region
.next_node_4k_mrf_impl: inc rbx; jmp .node_4k_loop_mrf_impl
.mark_large_mrf_impl: mov rax, r14; mov rdx, r15; mov r8, rax; and r8, [page_size_2m_minus_1]; jz .s2m_a_mrf_impl; mov r10, [page_size_2m_minus_1]; not r10; and rax, r10; add rax, PAGE_SIZE_2M; .s2m_a_mrf_impl: mov rsi, rdx; and rsi, [page_size_2m_minus_1]; jz .e2m_a_mrf_impl; mov r10, [page_size_2m_minus_1]; not r10; and rdx, r10; .e2m_a_mrf_impl: cmp rax, rdx; jae .done_mark_free_mrf_impl
    mov r12, rax; mov r13, rdx; movzx rcx, byte [numa_node_count]; xor rbx, rbx
.node_2m_loop_mrf_impl: cmp rbx, rcx; jae .done_mark_free_mrf_impl; mov rdi, [pmm_node_large_bitmaps + rbx*8]; test rdi, rdi; jz .next_node_2m_mrf_impl; mov r8, [pmm_node_large_frame_counts + rbx*8]; test r8, r8; jz .next_node_2m_mrf_impl; mov r10, [pmm_node_base_addrs + rbx*8]; mov r11, [pmm_node_addr_limits + rbx*8]; mov rax, r12; cmp rax, r10; cmovb rax, r10; mov rdx, r13; cmp rdx, r11; cmova rdx, r11; cmp rax, rdx; jae .next_node_2m_mrf_impl; sub rax, r10; sub rdx, r10; shr rax, 21; shr rdx, 21; mov r9, 0; call mark_bitmap_region
.next_node_2m_mrf_impl: inc rbx; jmp .node_2m_loop_mrf_impl
.done_mark_free_mrf_impl: pop r15;pop r14;pop r13;pop r12;pop r11;pop r10;pop r9;pop r8;pop rdi;pop rsi;pop rdx;pop rcx;pop rbx; ret

pmm_alloc_frame_node: ; RCX=NodeID (-1 for any)
    push rbx; push rcx; push rdx; push rsi; push rdi; push r8; push r9; push r10; push r11
    mov r10, rcx; movzx r11, byte [numa_node_count]; cmp r10, -1; jne .pafn_specific_node_check
.pafn_any_node_search: xor rsi, rsi; jmp .pafn_node_loop_start
.pafn_specific_node_check: cmp r10, r11; jae .pafn_fail_alloc_4k; mov rsi, r10
.pafn_node_loop_start: cmp rsi, r11; jae .pafn_fail_alloc_4k_search_end; mov rdi, [pmm_node_bitmaps + rsi*8]; test rdi, rdi; jz .pafn_next_node; mov rdx, [pmm_node_frame_counts + rsi*8]; test rdx, rdx; jz .pafn_next_node; mov r9, [pmm_node_base_addrs + rsi*8]; mov rcx, [last_alloc_index_4k + rsi*8]; mov rbx, rdx; add rbx, 63; shr rbx, 6
.pafn_scan_loop: cmp rcx, rbx; jae .pafn_wrap_scan; mov rax, [rdi + rcx*8]; cmp rax, -1; je .pafn_next_qword; not rax; bsf rax, rax; mov r8, rcx; shl r8, 6; add r8, rax; cmp r8, rdx; jae .pafn_next_qword; mov rbx, 1; shl rbx, al; LOCK_PREFIX bts [rdi + rcx*8], rax; jc .pafn_scan_loop; mov [last_alloc_index_4k + rsi*8], rcx; inc qword [pmm_used_frames]; mov rax, r8; shl rax, 12; add rax, r9; jmp .pafn_done_alloc_4k
.pafn_next_qword: inc rcx; jmp .pafn_scan_loop
.pafn_wrap_scan: cmp qword [last_alloc_index_4k + rsi*8], 0; je .pafn_next_node; mov qword [last_alloc_index_4k + rsi*8], 0; xor rcx, rcx; jmp .pafn_scan_loop
.pafn_next_node: cmp r10, -1; jne .pafn_fail_alloc_4k; inc rsi; jmp .pafn_node_loop_start
.pafn_fail_alloc_4k_search_end:
.pafn_fail_alloc_4k: xor rax, rax
.pafn_done_alloc_4k: pop r11; pop r10; pop r9; pop r8; pop rdi; pop rsi; pop rdx; pop rcx; pop rbx; ret

pmm_alloc_frame: mov rcx, -1; jmp pmm_alloc_frame_node

pmm_free_frame_node: ; RDI=Address
    push rbx; push rcx; push rdx; push rsi; push rdi; push r8; push r9
    mov rax, rdi; and rax, [page_size_4k_minus_1]; jnz .pffn_err_align; call pmm_get_node_for_addr; mov rsi, rax; mov rdi, [pmm_node_bitmaps + rsi*8]; test rdi, rdi; jz .pffn_err_range; mov r9, [pmm_node_base_addrs + rsi*8]; mov rcx, [pmm_node_frame_counts + rsi*8]; pop rax; sub rax, r9; shr rax, 12; cmp rax, rcx; jae .pffn_err_range; mov r8, rax; mov rbx, rax; shr rbx, 6; and r8b, 63; LOCK_PREFIX btr [rdi + rbx*8], r8; jnc .pffn_err_notset; dec qword [pmm_used_frames]; xor rax, rax; jmp .pffn_done
.pffn_err_align: mov rax, 1; jmp .pffn_done_pop
.pffn_err_range: mov rax, 2; jmp .pffn_done_pop
.pffn_err_notset: mov rax, 3
.pffn_done: pop r9; pop r8; pop rdi; pop rsi; pop rdx; pop rcx; pop rbx; ret
.pffn_done_pop: pop r9; pop r8; pop rdi; pop rsi; pop rdx; pop rcx; pop rbx; ret

pmm_free_frame: jmp pmm_free_frame_node

pmm_alloc_large_frame: ; RCX=NodeID (-1 for any)
    push rbx; push rcx; push rdx; push rsi; push rdi; push r8; push r9; push r10; push r11
    mov r10, rcx; movzx r11, byte [numa_node_count]; cmp r10, -1; jne .palf_specific_node_check; .palf_any_node_search: xor rsi, rsi; jmp .palf_node_loop_start
.palf_specific_node_check: cmp r10, r11; jae .palf_fail_alloc_large; mov rsi, r10
.palf_node_loop_start: cmp rsi, r11; jae .palf_fail_alloc_large_search_end; mov rdi, [pmm_node_large_bitmaps + rsi*8]; test rdi, rdi; jz .palf_next_node; mov rdx, [pmm_node_large_frame_counts + rsi*8]; test rdx, rdx; jz .palf_next_node; mov r9, [pmm_node_base_addrs + rsi*8]; mov rcx, [last_alloc_index_large + rsi*8]; mov rbx, rdx; add rbx, 63; shr rbx, 6
.palf_scan_loop: cmp rcx, rbx; jae .palf_wrap_scan; mov rax, [rdi + rcx*8]; cmp rax, -1; je .palf_next_qword; not rax; bsf rax, rax; mov r8, rcx; shl r8, 6; add r8, rax; cmp r8, rdx; jae .palf_next_qword; mov rbx, 1; shl rbx, al; LOCK_PREFIX bts [rdi + rcx*8], rax; jc .palf_scan_loop; mov [last_alloc_index_large + rsi*8], rcx; inc qword [pmm_used_large_frames]; mov rax, r8; shl rax, 21; add rax, r9; jmp .palf_done_alloc_large
.palf_next_qword: inc rcx; jmp .palf_scan_loop
.palf_wrap_scan: cmp qword [last_alloc_index_large + rsi*8], 0; je .palf_next_node; mov qword [last_alloc_index_large + rsi*8], 0; xor rcx, rcx; jmp .palf_scan_loop
.palf_next_node: cmp r10, -1; jne .palf_fail_alloc_large; inc rsi; jmp .palf_node_loop_start
.palf_fail_alloc_large_search_end:
.palf_fail_alloc_large: xor rax, rax
.palf_done_alloc_large: pop r11; pop r10; pop r9; pop r8; pop rdi; pop rsi; pop rdx; pop rcx; pop rbx; ret

pmm_free_large_frame: ; RDI=Address
    push rbx; push rcx; push rdx; push rsi; push rdi; push r8; push r9
    mov rax, rdi; and rax, [page_size_2m_minus_1]; jnz .pflf_err_align; call pmm_get_node_for_addr; mov rsi, rax; mov rdi, [pmm_node_large_bitmaps + rsi*8]; test rdi, rdi; jz .pflf_err_range; mov r9, [pmm_node_base_addrs + rsi*8]; mov rcx, [pmm_node_large_frame_counts + rsi*8]; pop rax; sub rax, r9; shr rax, 21; cmp rax, rcx; jae .pflf_err_range; mov r8, rax; mov rbx, rax; shr rbx, 6; and r8b, 63; LOCK_PREFIX btr [rdi + rbx*8], r8; jnc .pflf_err_notset; dec qword [pmm_used_large_frames]; xor rax, rax; jmp .pflf_done
.pflf_err_align: mov rax, 1; jmp .pflf_done_pop
.pflf_err_range: mov rax, 2; jmp .pflf_done_pop
.pflf_err_notset: mov rax, 3
.pflf_done: pop r9; pop r8; pop rdi; pop rsi; pop rdx; pop rcx; pop rbx; ret
.pflf_done_pop: pop r9; pop r8; pop rdi; pop rsi; pop rdx; pop rcx; pop rbx; ret
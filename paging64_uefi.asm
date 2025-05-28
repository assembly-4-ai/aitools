; paging64_uefi.asm: UEFI-centric 64-bit Paging Setup
; Depends on: boot_defs_temp.inc (with UEFI defs)
; Depends on: pmm64_uefi.asm (for pmm_alloc_frame)

BITS 64
%include "boot_defs_temp.inc"

global paging_init_64_uefi
global map_page
global unmap_page
global map_2mb_page
global unmap_2mb_page
global reload_cr3
global kernel_pml4         ; Expose the variable holding the final PML4 address

extern pmm_alloc_frame ; Use PMM for page table allocation now
extern _kernel_start   ; Defined by linker script
extern _kernel_end     ; Defined by linker script

section .data align=8
kernel_pml4 dq 0
page_size_4k_minus_1 dq PAGE_SIZE_4K - 1
page_size_2m_minus_1 dq PAGE_SIZE_2M - 1

section .text

;-----------------------------------------------------------------------------
zero_page_paging: ; Helper to zero 4KB page (local version to avoid extern if simple)
                  ; Input RDI = Address
    push rdi
    push rcx
    push rax
    mov rcx, PAGE_SIZE_4K / 8
    xor eax, eax
    rep stosq
    pop rax
    pop rcx
    pop rdi
    ret

;-----------------------------------------------------------------------------
pmm_alloc_zeroed_page_paging: ; Allocates and zeroes using PMM (local for paging)
                              ; Output RAX = PhysAddr or 0
    mov rcx, -1 ; Request any node
    call pmm_alloc_frame ; RAX = Physical address or 0
    test rax, rax
    jz .pazp_alloc_fail
    push rax
    mov rdi, rax
    call zero_page_paging
    pop rax
    ret
.pazp_alloc_fail:
    ; RAX is already 0
    ret

;-----------------------------------------------------------------------------
reload_cr3:
    mov rax, cr3
    mov cr3, rax
    ret

;-----------------------------------------------------------------------------
map_page_internal: ; Input: RDI=Virt, RSI=Phys, RDX=Flags, R12=PageSizeFlag(0/PTE_PS)
                   ; Output: RAX: 0=success, 1=error
    push rdi ; Save original RDI (VirtAddr)
    push rbx
    push rcx
    push rdx ; Save original RDX (Flags)
    push rsi ; Save original RSI (PhysAddr)
    push r8
    push r9
    push r10
    push r11
    push r12 ; Save original R12 (PageSizeFlag)
    push r13

    mov r13, rdx ; Store original flags (RDX) into R13 for later use

    mov rax, cr3 ; Get current PML4 base address
    mov r8, rax
    and r8, 0xFFFFFFFFFFFFF000 ; Mask out flags (Explicit Hex for ~0xFFF)

    ; --- Level 4: PML4 ---
    mov rax, rdi ; Original RDI (Virtual Address)
    shr rax, 39
    and rax, 0x1FF ; PML4 Index
    lea r10, [r8 + rax*8] ; Address of PML4 entry
    mov r9, [r10] ; Read PML4 entry
    test r9, PTE_PRESENT
    jnz .mpi_pdpt

    call pmm_alloc_zeroed_page_paging ; RAX = PhysAddr of new PDPT or 0
    test rax, rax
    jz .mpi_err_map_internal
    mov r9, rax ; New PDPT physical address
    or r9, PTE_PRESENT | PTE_WRITABLE | PTE_USER ; Flags for PML4E
    mov [r10], r9 ; Write back new PML4 entry

.mpi_pdpt:
    ; --- Level 3: PDPT ---
    test r9, PTE_PS ; Check if this PML4E points to a 1GB page
    jnz .mpi_err_map_internal ; Cannot map if it's a 1GB page
    mov r8, r9
    and r8, 0xFFFFFFFFFFFFF000 ; Get PDPT base address

    mov rax, rdi ; Original RDI (Virtual Address)
    shr rax, 30
    and rax, 0x1FF ; PDPT Index
    lea r10, [r8 + rax*8] ; Address of PDPT entry
    mov r9, [r10] ; Read PDPT entry
    test r9, PTE_PRESENT
    jnz .mpi_pd

    call pmm_alloc_zeroed_page_paging ; RAX = PhysAddr of new PD or 0
    test rax, rax
    jz .mpi_err_map_internal
    mov r9, rax ; New PD physical address
    or r9, PTE_PRESENT | PTE_WRITABLE | PTE_USER ; Flags for PDPTE
    mov [r10], r9 ; Write back new PDPT entry

.mpi_pd:
    ; --- Level 2: PD ---
    test r9, PTE_PS ; Check if this PDPTE points to a 2MB page (already a large page)
    jnz .mpi_err_if_4k_over_2m ; Error if trying to map 4K page here or map 2MB over 2MB

    mov r8, r9
    and r8, 0xFFFFFFFFFFFFF000 ; Get PD base address

    mov rax, rdi ; Original RDI (Virtual Address)
    shr rax, 21
    and rax, 0x1FF ; PD Index
    lea r10, [r8 + rax*8] ; R10 = Address of Page Directory Entry (PDE)

    mov r12, [rsp + 8*3] ; Get original R12 (PageSizeFlag) from stack (adjust offset for pushes)
    cmp r12, 0 ; Are we mapping a 4K page?
    jne .mpi_map_final_2m ; No, jump to map 2MB page

    ; --- Mapping 4K Page ---
    mov r9, [r10] ; Read PDE
    test r9, PTE_PS ; Check if PDE itself is a 2MB page
    jnz .mpi_err_map_internal ; Cannot map 4K if this PDE is for 2MB page
    test r9, PTE_PRESENT
    jnz .mpi_pt

    call pmm_alloc_zeroed_page_paging ; RAX = PhysAddr of new PT or 0
    test rax, rax
    jz .mpi_err_map_internal
    mov r9, rax ; New PT physical address
    or r9, PTE_PRESENT | PTE_WRITABLE | PTE_USER ; Flags for PDE
    mov [r10], r9 ; Write back new PDE

.mpi_pt:
    ; --- Level 1: PT ---
    mov r8, r9
    and r8, 0xFFFFFFFFFFFFF000 ; Get PT base address

    mov rax, rdi ; Original RDI (Virtual Address)
    shr rax, 12
    and rax, 0x1FF ; PT Index
    lea r10, [r8 + rax*8] ; R10 = Address of Page Table Entry (PTE)

    mov rbx, [rsp + 8*7] ; Get original RSI (Physical Address) from stack
    mov rdx, r13         ; Get original RDX (Flags) from R13
    or rbx, PTE_PRESENT
    or rbx, rdx ; OR with original flags
    and rbx, 0xFFFFFFFFFFFFFF7F ; Explicit ~PTE_PS (ensure PS bit is not set for 4K)
    mov [r10], rbx
    jmp .mpi_success_map_internal

.mpi_map_final_2m:
    ; --- Mapping 2MB Page ---
    ; R10 holds address of PDE
    mov r9, [r10] ; Read current PDE value
    test r9, PTE_PRESENT ; Check if it already points to a 4K page table
    jnz .mpi_err_map_internal ; Cannot map 2MB if 4K PT already exists here

    mov rbx, [rsp + 8*7] ; Get original RSI (Physical Address)
    mov rdx, r13         ; Get original RDX (Flags)
    or rbx, PTE_PRESENT | PTE_PS
    or rbx, rdx ; OR with original flags
    mov [r10], rbx
    jmp .mpi_success_map_internal

.mpi_err_if_4k_over_2m:
    mov r12, [rsp + 8*3] ; Get original R12 (PageSizeFlag)
    cmp r12, 0
    je .mpi_err_map_internal ; Trying to map 4K page over an existing 2MB/1GB page
    ; If trying to map 2MB over an existing 2MB, this is also an error
    ; unless an update mechanism is implemented. For now, treat as error.
    jmp .mpi_err_map_internal

.mpi_success_map_internal:
    mov rax, 0 ; Success code
    jmp .mpi_exit_map_internal
.mpi_err_map_internal:
    mov rax, 1 ; Error code
.mpi_exit_map_internal:
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi ; Restore original RSI
    pop rdx ; Restore original RDX
    pop rcx
    pop rbx
    pop rdi ; Restore original RDI for invlpg
    test rax, rax
    jnz .mpi_ret_no_invlpg
    invlpg [rdi]
.mpi_ret_no_invlpg:
    ret

;-----------------------------------------------------------------------------
map_page:
    push rbx
    push r12
    push r13
    mov rax, rdi
    or rax, rsi
    and rax, [page_size_4k_minus_1]
    jnz .mp_align_err_4k
    mov r12, 0 ; PageSizeFlag for 4K
    call map_page_internal
    jmp .mp_done_4k
.mp_align_err_4k:
    mov rax, 1
.mp_done_4k:
    pop r13
    pop r12
    pop rbx
    ret

;-----------------------------------------------------------------------------
map_2mb_page:
    push rbx
    push r12
    push r13
    mov rax, rdi
    or rax, rsi
    and rax, [page_size_2m_minus_1]
    jnz .mp_align_err_2m
    mov r12, PTE_PS ; PageSizeFlag for 2MB
    call map_page_internal
    jmp .mp_done_2m
.mp_align_err_2m:
    mov rax, 1
.mp_done_2m:
    pop r13
    pop r12
    pop rbx
    ret

;-----------------------------------------------------------------------------
unmap_page:
    push rbx; push rcx; push r10; push rdi
    mov rax, rdi; and rax, [page_size_4k_minus_1]; jnz .up_err_unmap_4k
    mov rax, cr3; mov r8, rax; and r8, 0xFFFFFFFFFFFFF000
    mov rax, rdi; shr rax, 39; and rax, 0x1FF; lea r10, [r8+rax*8]; mov r9, [r10]; test r9,PTE_PRESENT; jz .up_err_unmap_4k
    test r9, PTE_PS; jnz .up_err_unmap_4k; mov r8,r9; and r8,0xFFFFFFFFFFFFF000
    mov rax, rdi; shr rax, 30; and rax, 0x1FF; lea r10, [r8+rax*8]; mov r9, [r10]; test r9,PTE_PRESENT; jz .up_err_unmap_4k
    test r9, PTE_PS; jnz .up_err_unmap_4k; mov r8,r9; and r8,0xFFFFFFFFFFFFF000
    mov rax, rdi; shr rax, 21; and rax, 0x1FF; lea r10, [r8+rax*8]; mov r9, [r10]; test r9,PTE_PRESENT; jz .up_err_unmap_4k
    test r9, PTE_PS; jnz .up_err_unmap_4k; mov r8,r9; and r8,0xFFFFFFFFFFFFF000
    mov rax, rdi; shr rax, 12; and rax, 0x1FF; lea r10, [r8+rax*8]; mov rbx,[r10]; test rbx,PTE_PRESENT; jz .up_err_unmap_4k
    and rbx, 0xFFFFFFFFFFFFFFFE; mov [r10],rbx; invlpg [rdi]; xor rax,rax; jmp .up_done_unmap_4k
.up_err_unmap_4k: mov rax,1
.up_done_unmap_4k: pop rdi; pop r10; pop rcx; pop rbx; ret

;-----------------------------------------------------------------------------
unmap_2mb_page:
    push rbx; push rcx; push r10; push rdi
    mov rax, rdi; and rax, [page_size_2m_minus_1]; jnz .u2p_err_unmap_2m
    mov rax, cr3; mov r8, rax; and r8, 0xFFFFFFFFFFFFF000
    mov rax, rdi; shr rax, 39; and rax, 0x1FF; lea r10, [r8+rax*8]; mov r9, [r10]; test r9,PTE_PRESENT; jz .u2p_err_unmap_2m
    test r9, PTE_PS; jnz .u2p_err_unmap_2m; mov r8,r9; and r8,0xFFFFFFFFFFFFF000
    mov rax, rdi; shr rax, 30; and rax, 0x1FF; lea r10, [r8+rax*8]; mov r9, [r10]; test r9,PTE_PRESENT; jz .u2p_err_unmap_2m
    test r9, PTE_PS; jz .u2p_err_unmap_2m; mov r8,r9; and r8,0xFFFFFFFFFFFFF000
    mov rax, rdi; shr rax, 21; and rax, 0x1FF; lea r10, [r8+rax*8]
    mov r9,[r10]; test r9,PTE_PRESENT; jz .u2p_err_unmap_2m; test r9,PTE_PS; jz .u2p_err_unmap_2m
    and r9, 0xFFFFFFFFFFFFFFFE; mov [r10],r9; invlpg [rdi]; xor rax,rax; jmp .u2p_done_unmap_2m
.u2p_err_unmap_2m: mov rax,1
.u2p_done_unmap_2m: pop rdi; pop r10; pop rcx; pop rbx; ret

;-----------------------------------------------------------------------------
paging_init_64_uefi: ; Input: RCX=MapPtr, RDX=MapSize, R8=DescSize, R9=PtrToPML4Var
    push rbp
    mov rbp, rsp
    sub rsp, 8 ; Align stack
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rcx ; UEFI Map Buffer Ptr
    mov r13, rdx ; UEFI Map Size
    mov r14, r8  ; UEFI Descriptor Size
    mov r15, r9  ; Pointer to variable to store PML4 address

    ; 1. Allocate and Zero PML4 Table using PMM
    call pmm_alloc_zeroed_page_paging ; RAX = PML4 phys addr or 0
    test rax, rax
    jz .pi64u_fail_alloc_page
    mov [r15], rax        ; Store PML4 address in caller's variable
    mov [kernel_pml4], rax ; Store in our global for map_page_internal
    mov cr3, rax           ; Load new PML4

    ; 2. Map the Kernel
    lea rbx, [_kernel_start]
    lea rcx, [_kernel_end]
    mov rax, [page_size_2m_minus_1]
    not rax ; Create alignment mask
    and rbx, rax ; Align kernel_start down
    add rcx, PAGE_SIZE_2M ; Ensure end is covered
    and rcx, rax ; Align kernel_end down (effectively up due to previous add)
.pi64u_kmap_loop:
    cmp rbx, rcx
    jae .pi64u_kmap_done
    mov rdi, rbx ; Virtual = Physical
    mov rsi, rbx
    mov rdx, KERNEL_PTE_FLAGS
    call map_2mb_page
    test rax, rax
    jnz .pi64u_fail_map
    add rbx, PAGE_SIZE_2M
    jmp .pi64u_kmap_loop
.pi64u_kmap_done:

    ; 3. Map Usable Conventional Memory from UEFI Map
    mov rbx, r12 ; RBX = Current Descriptor Ptr (reset to start of map)
    mov rcx, r13 ; RCX = Remaining map size (reset)
    mov r10, r14 ; R10 = Descriptor size
.pi64u_memmap_loop:
    cmp rcx, r10 ; Check if enough bytes remain
    jl .pi64u_memmap_done

    mov edi, [rbx + EFI_MEMORY_DESCRIPTOR.Type]
    cmp edi, EfiConventionalMemory
    jne .pi64u_next_descriptor

    ; This is EfiConventionalMemory
    mov rdi, [rbx + EFI_MEMORY_DESCRIPTOR.PhysicalStart]
    mov rsi, [rbx + EFI_MEMORY_DESCRIPTOR.NumberOfPages]

    ; Calculate physical end (exclusive) for this descriptor
    mov rax, rsi ; NumberOfPages
    shl rax, 12  ; Size in bytes
    add rax, rdi ; Physical End = PhysicalStart + Size
    push rax     ; Save Physical End of descriptor

    ; Align PhysicalStart (RDI) down to 2MB boundary for loop start
    mov rax, [page_size_2m_minus_1]
    not rax
    and rdi, rax ; RDI = Loop CurrentPhysAddr (aligned down)

    pop r8       ; R8 = Physical End of descriptor (exclusive)
                 ; Align this end address UP then DOWN for loop end
    add r8, PAGE_SIZE_2M -1 ; Add (2M-1) for rounding up
    and r8, rax  ; R8 = Loop EndPhysAddr (aligned down, effectively up)

.pi64u_phys_loop:
    cmp rdi, r8 ; Compare CurrentPhysAddr with LoopEndPhysAddr
    jae .pi64u_next_descriptor ; Finished this descriptor

    ; Check if RDI (current 2MB page to map) overlaps with kernel
    push rax ; Save alignment mask

    mov rax, [page_size_2m_minus_1]; not rax ; R9 has alignment mask
    mov r9, rax

    lea rax, [_kernel_start]
    and rax, r9 ; Aligned kernel start
    cmp rdi, rax
    jb .pi64u_map_phys ; If current page is before kernel, map it

    lea rax, [_kernel_end]
    add rax, PAGE_SIZE_2M -1 ; Round up kernel end
    and rax, r9 ; Aligned kernel end
    cmp rdi, rax
    jae .pi64u_map_phys ; If current page is after kernel, map it

    pop rax ; Restore alignment mask
    jmp .pi64u_skip_phys ; Overlaps kernel, skip

.pi64u_map_phys:
    pop rax ; Restore alignment mask (not strictly needed now)
    mov rsi, rdi ; Physical address to map
    mov rdx, DATA_PTE_FLAGS
    call map_2mb_page ; RDI is Virt (identity mapped), RSI is Phys
    test rax, rax
    jnz .pi64u_fail_map
.pi64u_skip_phys:
    add rdi, PAGE_SIZE_2M ; Next 2MB chunk
    jmp .pi64u_phys_loop

.pi64u_next_descriptor:
    add rbx, r10 ; Move to next descriptor
    sub rcx, r10 ; Decrement remaining map size
    jmp .pi64u_memmap_loop
.pi64u_memmap_done:

    xor rax, rax ; Success
    jmp .pi64u_exit

.pi64u_fail_alloc_page:
    mov rax, 2 ; Error code 2 for allocation failure
    jmp .pi64u_exit_final
.pi64u_fail_map:
    mov rax, 1 ; Error code 1 for mapping failure
.pi64u_exit:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 8 ; Balance stack
    pop rbp
.pi64u_exit_final:
    ret
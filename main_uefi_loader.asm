; main_uefi_loader.asm - Merged UEFI Bootloader + GDT/IDT/PIC Setup

BITS 64

global _start
%include "boot_defs_temp.inc" ; Includes UEFI/AHCI/FAT/PIC Defs now

; --- Externals ---

;extern pmm64_init_uefi, pmm_alloc_frame, pmm_free_frame, pmm_mark_region_used
 ;Paging
;extern paging_init_64_uefi, reload_cr3, kernel_pml4
;GDT
;extern setup_final_gdt64, load_gdt_and_segments64, gdt64_pointer_var
;IDT
extern setup_final_idt64
extern load_idt64
extern dt64_pointer_var ; Use combined setup
; Screen Driver (Post-BS)
;extern scr64_init, scr64_print_string, scr64_print_hex, scr64_print_dec
; Keyboard Driver (Post-BS)
;extern keyboard_init, getchar_from_buffer
; PCI
;extern pci_init, pci_find_ahci_controller
; AHCI
;extern ahci_init, ahci_read_sectors, ahci_write_sectors
; FAT32
extern fat32_init
;PIC
extern pic_remap
; Payload / Panic
;extern shell_run, panic64
; 

;extern gop_pixel_format, gop_v_res, gop_framebuffer_size


; Globals defined elsewhere or by linker
global efi_image_base, efi_image_size
;global numa_node_count
global pmm_node_base_addrs, pmm_node_addr_limits
global key_buffer, key_buffer_head, key_buffer_tail ; Keyboard buffer

section .data align=64
    gImageHandle dq 0
    gSystemTable dq 0
    gMemoryMap dq 0             ; Pointer to allocated map buffer
    gMemoryMapSize dq 4096 * 4  ; Initial map buffer size (updated by GetMemoryMap)
    gMapKey dq 0
    gDescriptorSize dq 0
    gDescriptorVersion dq 0
    gFinalGdtBase dq 0
    gFinalIdtBase dq 0
    gAhciBaseAddr dq 0          ; Store found AHCI BAR here
    gFat32PartitionLba dq 0     ; Store found FAT32 LBA start here
    gAhciPortNum dd 0           ; Default AHCI port to use
    gFileSystemHandle dq 0      ; Handle for SimpleFileSystem protocol
    gFileSystemProtocol dq 0    ; SimpleFileSystem protocol interface
    gRootDirectory dq 0         ; Root directory file handle


    ;------------------------------------
    ; screen_gop defs
    ;------------------------------------
 
    section .data 
    

    ; Default NUMA info
    ;numa_node_count db 1
    ;pmm_node_base_addrs dq 0, 0, 0, 0, 0, 0, 0, 0
    ;pmm_node_addr_limits dq 0xFFFFFFFFFFFFFFFF, 0, 0, 0, 0, 0, 0, 0

    ; Messages (keep as before)
    msg_welcome db "UEFI Loader Initializing...", 0Dh, 0Ah, 0
    msg_loaded_image_ok db "Loaded Image Info OK", 0Dh, 0Ah, 0
    msg_loaded_image_fail db "Failed to get Loaded Image Protocol!", 0Dh, 0Ah, 0
    msg_gop_ok db "Graphics Output Protocol OK", 0Dh, 0Ah, 0
    msg_gop_fail db "Failed to get GOP!", 0Dh, 0Ah, 0
    msg_memmap_ok db "Memory Map OK", 0Dh, 0Ah, 0
    msg_memmap_fail db "Failed to get Memory Map!", 0Dh, 0Ah, 0
    msg_alloc_gdt_fail db "Failed to allocate GDT!", 0Dh, 0Ah, 0
    msg_alloc_idt_fail db "Failed to allocate IDT!", 0Dh, 0Ah, 0
    msg_pmm_ok db "PMM Initialized", 0Dh, 0Ah, 0
    msg_pmm_fail db "PMM Initialization Failed!", 0Dh, 0Ah, 0
    msg_paging_ok db "Paging Initialized", 0Dh, 0Ah, 0
    msg_paging_fail db "Paging Initialization Failed!", 0Dh, 0Ah, 0
    msg_gdt_ok db "GDT Setup OK", 0Dh, 0Ah, 0
    msg_exit_bs_fail db "ExitBootServices Failed!", 0Dh, 0Ah, 0
    msg_post_exit db "Exited Boot Services. Setting up HW...", 0Dh, 0Ah, 0
    msg_pic_ok db "PIC Remapped OK", 0Dh, 0Ah, 0
    msg_pci_ok db "PCI Init OK", 0Dh, 0Ah, 0
    msg_ahci_ok db "AHCI Init OK", 0Dh, 0Ah, 0
    msg_ahci_fail db "AHCI Init Failed!", 0Dh, 0Ah, 0
    msg_gpt_ok db "FAT32 Partition Found OK", 0Dh, 0Ah, 0
    msg_gpt_fail db "FAT32 Partition Not Found!", 0Dh, 0Ah, 0
    msg_fat32_ok db "FAT32 Init OK", 0Dh, 0Ah, 0
    msg_fat32_fail db "FAT32 Init Failed!", 0Dh, 0Ah, 0
    msg_kbd_ok db "Keyboard Init OK", 0Dh, 0Ah, 0
    msg_idt_ok db "IDT Setup OK. Enabling Interrupts.", 0Dh, 0Ah, 0
    msg_jumping db "Jumping to Shell...", 0Dh, 0Ah, 0
    msg_shell_return_err db "Shell Returned Unexpectedly!", 0Dh, 0Ah, 0
    msg_fs_ok db "File System Protocol OK", 0Dh, 0Ah, 0
    msg_fs_fail db "Failed to get File System Protocol!", 0Dh, 0Ah, 0
    msg_root_dir_ok db "Root Directory Opened OK", 0Dh, 0Ah, 0
    msg_root_dir_fail db "Failed to open Root Directory!", 0Dh, 0Ah, 0
    msg_file_open_fail db "Failed to open file!", 0Dh, 0Ah, 0
    msg_file_read_fail db "Failed to read file!", 0Dh, 0Ah, 0
    msg_file_write_fail db "Failed to write file!", 0Dh, 0Ah, 0
    msg_file_close_fail db "Failed to close file!", 0Dh, 0Ah, 0

    ; UEFI File I/O related GUIDs
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID: dd 0x964E5B22; dw 0x6459, 0x11D2; db 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B
    
    ; UEFI File I/O related constants
    %define EFI_FILE_MODE_READ    0x0000000000000001
    %define EFI_FILE_MODE_WRITE   0x0000000000000002
    %define EFI_FILE_MODE_CREATE  0x8000000000000000
    
    ; UEFI File I/O related offsets
    %define OFFSET_SFS_OPENVOLUME 0x08
    %define OFFSET_FILE_OPEN      0x08
    %define OFFSET_FILE_CLOSE     0x10
    %define OFFSET_FILE_READ      0x18
    %define OFFSET_FILE_WRITE     0x20
    %define OFFSET_FILE_GET_POS   0x28
    %define OFFSET_FILE_SET_POS   0x30
    
    section .bss align=4096
    initial_mmap_buffer resb 4096 * 4
    efi_image_base resq 1
    efi_image_size resq 1
    key_buffer resb 32
    key_buffer_head resb 4
    key_buffer_tail resb 4
    disk_read_buffer resb 4096

section .text
global efi_main


_start:
; --- Helper to unmask specific PIC IRQ line ---
pic_unmask_irq: ; Input: AL = IRQ line (0-15)
    push ax;
    push dx
    cmp al, 8
    jb .master_mask
.slave_mask:
    mov dx, PIC2_DATA ; Slave data port (0xA1)
    in al, dx         ; Read current mask
    mov ah, al        ; Save it
    pop dx            ; Get IRQ line back into dl
    sub dl, 8         ; Convert to slave line number (0-7)
    mov al, 1
    push cx
    mov cl, dl
    shl al, cl       ; Create bitmask
    pop cx
    not al            ; Invert mask (0 to unmask)
    and ah, al        ; Clear the specific bit in saved mask
    mov al, ah        ; Move new mask to AL
    mov dx, PIC2_DATA
    out dx, al        ; Write new mask to slave PIC
    jmp .mask_done
.master_mask:
    mov dx, PIC1_DATA ; Master data port (0x21)
    in al, dx         ; Read current mask
    mov ah, al        ; Save it
    pop dx            ; Get IRQ line back into dl
    mov al, 1
    push cx
    mov cl,dl
    shl al, cl        ; Create bitmask
    pop cx
    not al            ; Invert mask (0 to unmask)
    and ah, al        ; Clear the specific bit in saved mask
    mov al, ah        ; Move new mask to AL
    mov dx, PIC1_DATA
    out dx, al        ; Write new mask to master PIC
.mask_done:
    pop dx; pop ax
    ret


; --- Main UEFI Entry Point ---
efi_main:
    push rbp;
    mov rbp, rsp;
    sub rsp, 64;
    and rsp, -16
    push rbx;
    push rsi;
    push rdi;
    push r12;
    push r13;
    push r14;
    push r15
    mov [gImageHandle], rdi;
    mov [gSystemTable], rsi

    ; 1. Print Welcome
    mov rcx, [gSystemTable];
    mov rdx, msg_welcome;
    call UefiPrint

    ; 2. Get Loaded Image Info
    mov rcx, [gImageHandle];
    mov rdx, EFI_LOADED_IMAGE_PROTOCOL_GUID;
    mov r8, rsp;
    call UefiHandleProtocol
    test rax, rax;
    jnz .loaded_image_fail;
    mov rbx, [rsp];
    mov rax, [rbx + OFFSET_LOADED_IMAGE_IMAGEBASE];
    mov [efi_image_base], rax
    mov rax, [rbx + OFFSET_LOADED_IMAGE_IMAGESIZE];
    mov [efi_image_size], rax;
    mov rcx, [gSystemTable];
    mov rdx, msg_loaded_image_ok;
    call UefiPrint

    ; 3. Get GOP Info
    mov rcx, EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    mov rdx, 0;
    mov r8, rsp;
    call UefiLocateProtocol
    test rax, rax;
    jnz .gop_fail;
    mov rbx, [rsp];
    mov rcx, [rbx + OFFSET_GOP_MODE];
    mov rax, [rcx + OFFSET_GOP_MODE_FBBASE];
    mov [gop_framebuffer_base], rax
    mov rax, [rcx + OFFSET_GOP_MODE_FBSIZE];
    mov [gop_framebuffer_size], rax;
    mov rdx, [rcx + OFFSET_GOP_MODE_INFO];
    mov eax, [rdx + OFFSET_GOP_INFO_HRES]
    mov [gop_h_res], eax;
    mov eax, [rdx + OFFSET_GOP_INFO_VRES];
    mov [gop_v_res], eax;
    mov eax, [rdx + OFFSET_GOP_INFO_PIXELFMT];
    mov [gop_pixel_format], eax
    mov eax, [rdx + OFFSET_GOP_INFO_PIXELSPERSCANLINE] ; Use correct offset
    mov [gop_pixels_per_scanline], eax
    mov rcx, [gSystemTable];
    mov rdx, msg_gop_ok;
    call UefiPrint

    ; 3.5 Get File System Protocol
    mov rcx, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    mov rdx, 0;
    lea r8, [gFileSystemProtocol];
    call UefiLocateProtocol
    test rax, rax;
    jnz .fs_fail;
    
    ; Open Root Directory
    mov rbx, [gFileSystemProtocol];
    mov rax, [rbx + OFFSET_SFS_OPENVOLUME];
    mov rcx, rbx;
    lea rdx, [gRootDirectory];
    call rax
    test rax, rax;
    jnz .root_dir_fail;
    
    mov rcx, [gSystemTable];
    mov rdx, msg_root_dir_ok;
    call UefiPrint

    ; 4. Get Memory Map (First time)
    mov rdi, gMemoryMap;
    push rax
    xor rax,rax
    mov rax, initial_mmap_buffer
    mov [rdi], rax
    pop rax;
    mov rcx, rdi;
    mov rdx, gMemoryMapSize;
    mov r8, gMapKey;
    mov r9, gDescriptorSize;
    push gDescriptorVersion;
    call UefiGetMemoryMap
    pop rax;
    test rax, rax;
    jnz .memmap_fail;
    mov rcx, [gSystemTable];
    mov rdx, msg_memmap_ok;
    call UefiPrint

    ; 5. Allocate GDT/IDT memory
    mov rcx, AllocateAnyPages;
    mov rdx, EfiLoaderData;
    mov r8, 1;
    lea r9, [gFinalGdtBase];
    call UefiAllocatePages;
    test rax, rax;
    jnz .alloc_gdt_fail
    mov rcx, AllocateAnyPages;
    mov rdx, EfiLoaderData;
    mov r8, 1;
    lea r9, [gFinalIdtBase];
    call UefiAllocatePages;
    test rax, rax;
    jnz .alloc_idt_fail

    ; 6. Initialize PMM
    mov rcx, [gMemoryMap];
    mov rdx, [gMemoryMapSize];
    mov r8, [gDescriptorSize];
    call pmm64_init_uefi;
    test rax, rax;
    jnz .pmm_fail
    mov rax, [gFinalGdtBase];
    mov rdx, PAGE_SIZE_4K;
    add rdx, rax;
    call pmm_mark_region_used
    mov rax, [gFinalIdtBase];
    mov rdx, PAGE_SIZE_4K;
    add rdx, rax;
    call pmm_mark_region_used
    mov rax, [gop_framebuffer_base];
    mov rdx, [gop_framebuffer_size];
    add rdx, rax;
    call pmm_mark_region_used
    mov rcx, [gSystemTable];
    mov rdx, msg_pmm_ok;
    call UefiPrint

    ; 7. Load Kernel/Payload - Skipped

    ; 8. Setup Final Paging
    mov rcx, [gMemoryMap];
    mov rdx, [gMemoryMapSize];
    mov r8, [gDescriptorSize];
    mov r9, kernel_pml4;
    call paging_init_64_uefi;
    test rax, rax;
    jnz .paging_fail
    mov rcx, [gSystemTable];
    mov rdx, msg_paging_ok;
    call UefiPrint

    ; 9. Setup GDT Descriptors
    mov rdi, [gFinalGdtBase];
    call setup_final_gdt64;
    mov rcx, [gSystemTable];
    mov rdx, msg_gdt_ok;
    call UefiPrint

    ; 10. Get Memory Map AGAIN for ExitBootServices Key
    mov rcx, gMemoryMap;
    mov rdx, gMemoryMapSize;
    mov r8, gMapKey;
    mov r9, gDescriptorSize;
    push gDescriptorVersion;
    call UefiGetMemoryMap
    pop rax;
    test rax, rax;
    jnz .memmap_fail

    ; 11. Exit Boot Services
    mov rdi, [gImageHandle];
    mov rcx, rdi;
    mov rdx, [gMapKey];
    call UefiExitBootServices;
    test rax, rax;
    jnz .exit_bs_fail

    ; --- UEFI Boot Services are GONE ---

    ; 12. Load GDT/Segments, Init Screen
    call load_gdt_and_segments64
    mov rdi, [gop_framebuffer_base];
    mov esi, [gop_h_res];
    mov edx, [gop_v_res];
    mov ecx, [gop_pixels_per_scanline];
    mov r8d, [gop_pixel_format];
    call scr64_init
    mov rsi, msg_post_exit;
    call scr64_print_string

    ; 13. Remap PIC
    call pic_remap
    mov rsi, msg_pic_ok;
    call scr64_print_string

    ; 14. Setup IDT and Load
    mov rdi, [gFinalIdtBase];
    call setup_final_idt64
    call load_idt64
    mov rsi, msg_idt_ok; 
    call scr64_print_string

    ; 15. Initialize PCI & AHCI
    call pci_init ; msg_pci_ok
    call pci_find_ahci_controller;
    test rax, rax;
    jz .ahci_fail;
    mov [gAhciBaseAddr], rax
    call ahci_init ; Pass BAR in RAX from previous call
    test rax, rax;
    jnz .ahci_fail; ;msg_ahci_ok

    ; 16. Find FAT32 Partition (Placeholder)
    ; call FindFat32PartitionLba ; Needs implementation using AHCI
    ; test rax, rax; jnz .gpt_fail
    mov qword [gFat32PartitionLba], 2048 ;### HARCODED LBA FOR TESTING ###
    ; msg_gpt_ok

    ; 17. Initialize FAT32
    mov rdi, [gFat32PartitionLba];
    mov edx, [gAhciPortNum];
    call fat32_init
    test rax, rax;
    jnz .fat32_fail
    ; msg_fat32_ok

    ; 18. Initialize Keyboard & Unmask IRQ
    call keyboard_init
    mov al, KB_IRQ ; Unmask Keyboard IRQ (IRQ 1)
    call pic_unmask_irq
    ; msg_kbd_ok

    ; 19. Enable Interrupts
    sti

    ; 20. Jump to Shell
    mov rsi, msg_jumping;
    call scr64_print_string
    call shell_run

    ; Should not return
    mov rsi, msg_shell_return_err;
    call panic64

; --- Error Handling ---
.loaded_image_fail:
mov rsi, msg_loaded_image_fail; 
jmp .panic_or_print
.gop_fail: 
mov rsi, msg_gop_fail;
jmp .panic_or_print
.fs_fail:
mov rsi, msg_fs_fail;
jmp .panic_or_print
.root_dir_fail:
mov rsi, msg_root_dir_fail;
jmp .panic_or_print
.memmap_fail:
mov rsi, msg_memmap_fail;
jmp .panic_or_print
.alloc_gdt_fail:
mov rsi, msg_alloc_gdt_fail;
jmp .panic_or_print
.alloc_idt_fail:
mov rsi, msg_alloc_idt_fail;
jmp .panic_or_print
.pmm_fail:
mov rsi, msg_pmm_fail;
jmp .panic_or_print
.paging_fail:
mov rsi, msg_paging_fail;
jmp .panic_or_print
.exit_bs_fail:
jmp .halt_critical ;Cannot print ExitBS failure reliably
.ahci_fail:
mov rsi, msg_ahci_fail;
call scr64_print_string;
jmp .halt
.gpt_fail:
mov rsi, msg_gpt_fail;
call scr64_print_string;
jmp .halt
.fat32_fail:
mov rsi, msg_fat32_fail;
call scr64_print_string;
jmp .halt
.panic_or_print:
mov rcx, [gSystemTable];
mov rdx, rsi;
call UefiPrint;
jmp .halt
.halt_critical:; Halt when printing might not work
.halt:
cli;
.spin:
hlt;
jmp .spin
.clean_exit:
xor rax, rax;
pop r15;
pop r14;
pop r13;
pop r12;
pop rdi;
pop rsi;
pop rbx;
add rsp, 64;
pop rbp;
ret

;--------------------------------------------------------------------------
; UefiPrint: Prints a null-terminated Unicode string.
; Input: RCX = (Ignored, uses gSystemTable)
;        RDX = Pointer to Null-terminated Unicode String
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiPrint:
    push rbp
    mov rbp, rsp
    sub rsp, 32+8 ; Shadow space for 4 registers + 8 for alignment/scratch
    and rsp, -16  ; Ensure 16-byte alignment before call

    push rbx
    push rsi
    push rdi
    push r10 ; Using r10 for function pointer

    mov rsi, [gSystemTable]
    test rsi, rsi
    jz .print_fail_no_systable

    mov rbx, [rsi + OFFSET_ST_CONOUT]
    test rbx, rbx
    jz .print_fail_no_conout

    mov r10, [rbx + OFFSET_CONOUT_OUTPUTSTRING]
    test r10, r10
    jz .print_fail_no_func

    ; Args for ConOut->OutputString:
    ; RCX = This (ConOut Protocol Pointer)
    ; RDX = String (Passed as input RDX)
    mov rcx, rbx
    ; RDX is already set
    call r10
    ; RAX holds status
    jmp .print_done

.print_fail_no_systable:
    mov rax, EFI_INVALID_PARAMETER ; SystemTable not set
    jmp .print_done
.print_fail_no_conout:
.print_fail_no_func:
    mov rax, EFI_UNSUPPORTED
.print_done:
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiAllocatePages: Allocates physical memory pages.
; Input: RCX = AllocateType
;        RDX = MemoryType
;        R8  = Pages (Number of pages)
;        R9  = Address of variable to store result (Memory Ptr)
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiAllocatePages:
    push rbp
    mov rbp, rsp
    sub rsp, 32+8
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10

    mov rsi, [gSystemTable]
    test rsi, rsi
    jz .alloc_fail_no_systable

    mov rbx, [rsi + OFFSET_ST_BOOTSERVICES]
    test rbx, rbx
    jz .alloc_fail_no_bs

    mov r10, [rbx + OFFSET_BS_ALLOCATEPAGES]
    test r10, r10
    jz .alloc_fail_no_func

    ; Args for BS->AllocatePages are already in RCX, RDX, R8, R9 from caller
    call r10
    ; RAX holds status
    jmp .alloc_done

.alloc_fail_no_systable:
.alloc_fail_no_bs:
.alloc_fail_no_func:
    mov rax, EFI_NOT_READY
.alloc_done:
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiGetMemoryMap: Gets the UEFI memory map with robust buffer reallocation.
; Input:
;   RCX = Pointer to variable to store Map Buffer address
;   RDX = Pointer to variable to store Map Size (input: initial size, output: actual size)
;   R8  = Pointer to variable to store Map Key
;   R9  = Pointer to variable to store Descriptor Size
;   [RSP+40] (stack) = Pointer to variable to store Descriptor Version
; Output: RAX = Status. Variables pointed to by args are updated.
;
; This implementation handles EFI_BUFFER_TOO_SMALL by reallocating the buffer
; to the required size and retrying the operation.
;--------------------------------------------------------------------------
UefiGetMemoryMap:
    push rbp
    mov rbp, rsp
    sub rsp, 64       ; Shadow space + local vars + alignment
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Save input parameters for later use
    mov r12, rcx      ; Save Ptr to gMemMapVar
    mov r13, rdx      ; Save Ptr to gMemMapSizeVar
    mov r14, r8       ; Save Ptr to gMapKeyVar
    mov r15, r9       ; Save Ptr to gDescSizeVar
    mov r11, [rbp + 16 + (9*8)] ; Get DescriptorVersion Ptr from stack

    ; Get system table and boot services
    mov rsi, [gSystemTable]
    test rsi, rsi
    jz .getmap_fail_no_systable

    mov rbx, [rsi + OFFSET_ST_BOOTSERVICES]
    test rbx, rbx
    jz .getmap_fail_no_bs

    mov r10, [rbx + OFFSET_BS_GETMEMORYMAP]
    test r10, r10
    jz .getmap_fail_no_func

.retry_getmap:
    ; Prepare arguments for GetMemoryMap call
    mov rcx, r13      ; RCX = Ptr to MemMapSizeVar
    mov rdx, [r12]    ; RDX = Current buffer address
    mov r8, r14       ; R8 = Ptr to MapKeyVar
    mov r9, r15       ; R9 = Ptr to DescSizeVar
    push r11          ; Push DescriptorVersion Ptr for call
    
    ; Call GetMemoryMap
    call r10
    
    ; Clean up stack
    add rsp, 8
    
    ; Check if we got EFI_BUFFER_TOO_SMALL
    cmp rax, EFI_BUFFER_TOO_SMALL
    jne .getmap_check_done
    
    ; Handle buffer too small case
    ; First, free the old buffer if it's not the initial static buffer
    mov rdx, [r12]
    cmp rdx, initial_mmap_buffer
    je .skip_free_buffer
    
    ; Free the old buffer
    push rax          ; Save status
    mov rcx, rdx      ; RCX = Memory address to free
    mov rdx, [r13]    ; RDX = Size in bytes
    add rdx, PAGE_SIZE_4K - 1
    shr rdx, 12       ; Convert bytes to pages (round up)
    call UefiFreePages
    pop rax           ; Restore status
    
.skip_free_buffer:
    ; Calculate new buffer size in pages (round up)
    mov rdx, [r13]    ; Get updated size from EFI call
    add rdx, PAGE_SIZE_4K - 1
    shr rdx, 12       ; Convert bytes to pages (round up)
    
    ; Allocate new buffer
    mov rcx, AllocateAnyPages
    mov r8, rdx       ; R8 = Pages
    mov rdx, EfiLoaderData
    lea r9, [rsp-8]   ; Temporary storage for new address
    push rax          ; Save status
    call UefiAllocatePages
    pop rbx           ; Get old status in RBX
    
    ; Check allocation success
    test rax, rax
    jnz .getmap_fail_alloc
    
    ; Update buffer pointer
    mov rax, [rsp-8]  ; Get new buffer address
    mov [r12], rax    ; Update buffer pointer
    
    ; Try GetMemoryMap again
    jmp .retry_getmap
    
.getmap_check_done:
    ; Check if GetMemoryMap succeeded
    test rax, rax
    jnz .getmap_done  ; If error, return it
    
    ; Success - return EFI_SUCCESS
    xor rax, rax
    jmp .getmap_done
    
.getmap_fail_no_systable:
.getmap_fail_no_bs:
.getmap_fail_no_func:
    mov rax, EFI_NOT_READY
    jmp .getmap_done
    
.getmap_fail_alloc:
    mov rax, EFI_OUT_OF_RESOURCES
    
.getmap_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiFreePages: Frees physical memory pages.
; Input: RCX = Memory address to free
;        RDX = Number of pages to free
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiFreePages:
    push rbp
    mov rbp, rsp
    sub rsp, 32+8
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10

    mov rsi, [gSystemTable]
    test rsi, rsi
    jz .free_fail_no_systable

    mov rbx, [rsi + OFFSET_ST_BOOTSERVICES]
    test rbx, rbx
    jz .free_fail_no_bs

    mov r10, [rbx + OFFSET_BS_FREEPAGES]
    test r10, r10
    jz .free_fail_no_func

    ; Args for BS->FreePages are already in RCX, RDX from caller
    call r10
    ; RAX holds status
    jmp .free_done

.free_fail_no_systable:
.free_fail_no_bs:
.free_fail_no_func:
    mov rax, EFI_NOT_READY
.free_done:
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiHandleProtocol: Gets a protocol interface for a handle.
; Enhanced implementation with robust error handling and validation
; for interaction with found protocols.
;
; Input: RCX = Handle
;        RDX = Protocol GUID
;        R8  = Interface (Pointer to store result)
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiHandleProtocol:
    push rbp
    mov rbp, rsp
    sub rsp, 48          ; Shadow space + local vars + alignment
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10
    push r12
    push r13
    push r14
    push r15

    ; Save input parameters
    mov r12, rcx         ; Save Handle
    mov r13, rdx         ; Save Protocol GUID
    mov r14, r8          ; Save Interface pointer

    ; Validate input parameters
    test r12, r12
    jz .handle_fail_invalid_handle
    
    test r13, r13
    jz .handle_fail_invalid_guid
    
    test r14, r14
    jz .handle_fail_invalid_interface

    ; Get system table and boot services
    mov rsi, [gSystemTable]
    test rsi, rsi
    jz .handle_fail_no_systable

    mov rbx, [rsi + OFFSET_ST_BOOTSERVICES]
    test rbx, rbx
    jz .handle_fail_no_bs

    mov r10, [rbx + OFFSET_BS_HANDLEPROTOCOL]
    test r10, r10
    jz .handle_fail_no_func

    ; Prepare arguments for HandleProtocol call
    mov rcx, r12         ; RCX = Handle
    mov rdx, r13         ; RDX = Protocol GUID
    mov r8, r14          ; R8 = Interface pointer
    
    ; Call HandleProtocol
    call r10
    
    ; Check for errors
    test rax, rax
    jnz .handle_error_check
    
    ; Success - verify interface pointer is valid
    mov rcx, [r14]       ; Get interface pointer value
    test rcx, rcx
    jz .handle_fail_null_interface
    
    ; Return EFI_SUCCESS
    xor rax, rax
    jmp .handle_done
    
.handle_error_check:
    ; Handle specific error cases
    cmp rax, EFI_UNSUPPORTED
    je .handle_protocol_unsupported
    
    cmp rax, EFI_NOT_FOUND
    je .handle_protocol_not_found
    
    ; For other errors, just return the error code
    jmp .handle_done
    
.handle_protocol_unsupported:
    ; Protocol is not supported by the handle
    mov rax, EFI_UNSUPPORTED
    jmp .handle_done
    
.handle_protocol_not_found:
    ; Protocol not found on this handle
    mov rax, EFI_NOT_FOUND
    jmp .handle_done
    
.handle_fail_invalid_handle:
    ; Handle is NULL
    mov rax, EFI_INVALID_PARAMETER
    jmp .handle_done
    
.handle_fail_invalid_guid:
    ; Protocol GUID is NULL
    mov rax, EFI_INVALID_PARAMETER
    jmp .handle_done
    
.handle_fail_invalid_interface:
    ; Interface pointer is NULL
    mov rax, EFI_INVALID_PARAMETER
    jmp .handle_done
    
.handle_fail_null_interface:
    ; Interface pointer is NULL even though call succeeded
    mov rax, EFI_INVALID_PARAMETER
    jmp .handle_done
    
.handle_fail_no_systable:
.handle_fail_no_bs:
.handle_fail_no_func:
    mov rax, EFI_NOT_READY
    
.handle_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiLocateProtocol: Locates a protocol interface.
; Enhanced implementation with proper error handling and support for all required protocols
; (GOP, Loaded Image, BlockIO, SimpleFileSystem, etc.)
;
; Input: RCX = Protocol GUID
;        RDX = Registration (NULL)
;        R8  = Interface (Pointer to store result)
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiLocateProtocol:
    push rbp
    mov rbp, rsp
    sub rsp, 48          ; Shadow space + local vars + alignment
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10
    push r12
    push r13
    push r14
    push r15

    ; Save input parameters
    mov r12, rcx         ; Save Protocol GUID
    mov r13, rdx         ; Save Registration
    mov r14, r8          ; Save Interface pointer

    ; Get system table and boot services
    mov rsi, [gSystemTable]
    test rsi, rsi
    jz .locate_fail_no_systable

    mov rbx, [rsi + OFFSET_ST_BOOTSERVICES]
    test rbx, rbx
    jz .locate_fail_no_bs

    mov r10, [rbx + OFFSET_BS_LOCATEPROTOCOL]
    test r10, r10
    jz .locate_fail_no_func

    ; Prepare arguments for LocateProtocol call
    mov rcx, r12         ; RCX = Protocol GUID
    mov rdx, r13         ; RDX = Registration (usually NULL)
    mov r8, r14          ; R8 = Interface pointer
    
    ; Call LocateProtocol
    call r10
    
    ; Check for errors
    test rax, rax
    jnz .locate_handle_error
    
    ; Success - verify interface pointer is valid
    mov rcx, [r14]       ; Get interface pointer value
    test rcx, rcx
    jz .locate_fail_null_interface
    
    ; Return EFI_SUCCESS
    xor rax, rax
    jmp .locate_done
    
.locate_handle_error:
    ; Handle specific error cases if needed
    cmp rax, EFI_NOT_FOUND
    je .locate_protocol_not_found
    
    ; For other errors, just return the error code
    jmp .locate_done
    
.locate_protocol_not_found:
    ; Protocol not found - could implement retry logic here if needed
    mov rax, EFI_NOT_FOUND
    jmp .locate_done
    
.locate_fail_null_interface:
    ; Interface pointer is NULL even though call succeeded
    mov rax, EFI_INVALID_PARAMETER
    jmp .locate_done
    
.locate_fail_no_systable:
.locate_fail_no_bs:
.locate_fail_no_func:
    mov rax, EFI_NOT_READY
    
.locate_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiExitBootServices: Exits UEFI Boot Services.
; Input: RCX = ImageHandle, RDX = MapKey
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiExitBootServices:
    push rbp
    mov rbp, rsp
    sub rsp, 32+8
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10

    mov rsi, [gSystemTable]
    test rsi, rsi
    jz .exit_fail_no_systable

    mov rbx, [rsi + OFFSET_ST_BOOTSERVICES]
    test rbx, rbx
    jz .exit_fail_no_bs

    mov r10, [rbx + OFFSET_BS_EXITBOOTSERVICES]
    test r10, r10
    jz .exit_fail_no_func

    ; Args for BS->ExitBootServices are already in RCX, RDX from caller
    call r10
    ; RAX holds status
    jmp .exit_done

.exit_fail_no_systable:
.exit_fail_no_bs:
.exit_fail_no_func:
    mov rax, EFI_NOT_READY
.exit_done:
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiOpenFile: Opens a file using the UEFI File Protocol.
; Input: RCX = Directory handle (or NULL for root)
;        RDX = Filename (null-terminated Unicode string)
;        R8  = Open mode (EFI_FILE_MODE_READ, EFI_FILE_MODE_WRITE, etc.)
;        R9  = Attributes (only used when creating a file)
;        [RSP+40] = Address to store new file handle
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiOpenFile:
    push rbp
    mov rbp, rsp
    sub rsp, 48          ; Shadow space + local vars + alignment
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10
    push r12
    push r13
    push r14
    push r15

    ; Save input parameters
    mov r12, rcx         ; Save Directory handle
    mov r13, rdx         ; Save Filename
    mov r14, r8          ; Save Open mode
    mov r15, r9          ; Save Attributes
    
    ; If directory handle is NULL, use root directory
    test r12, r12
    jnz .use_provided_dir
    
    mov r12, [gRootDirectory]
    test r12, r12
    jz .open_fail_no_root
    
.use_provided_dir:
    ; Validate other parameters
    test r13, r13
    jz .open_fail_invalid_filename
    
    ; Get the Open function from the directory handle
    mov rbx, [r12]       ; Get the file protocol interface
    test rbx, rbx
    jz .open_fail_invalid_dir
    
    mov r10, [rbx + OFFSET_FILE_OPEN]
    test r10, r10
    jz .open_fail_no_func
    
    ; Prepare arguments for File->Open call
    mov rcx, r12         ; RCX = Directory handle
    lea rdx, [rbp + 16 + (4*8)] ; Get address of output parameter from stack
    mov r8, r13          ; R8 = Filename
    mov r9, r14          ; R9 = Open mode
    push r15             ; Push Attributes
    
    ; Call File->Open
    call r10
    
    ; Clean up stack
    add rsp, 8
    
    ; Check for errors
    test rax, rax
    jnz .open_done       ; If error, return it
    
    ; Success - verify file handle is valid
    mov rdx, [rbp + 16 + (4*8)] ; Get output parameter address
    mov rcx, [rdx]       ; Get file handle value
    test rcx, rcx
    jz .open_fail_null_handle
    
    ; Return EFI_SUCCESS
    xor rax, rax
    jmp .open_done
    
.open_fail_no_root:
    mov rax, EFI_NOT_READY
    jmp .open_done
    
.open_fail_invalid_filename:
.open_fail_invalid_dir:
.open_fail_null_handle:
    mov rax, EFI_INVALID_PARAMETER
    jmp .open_done
    
.open_fail_no_func:
    mov rax, EFI_UNSUPPORTED
    
.open_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiCloseFile: Closes a file handle.
; Input: RCX = File handle
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiCloseFile:
    push rbp
    mov rbp, rsp
    sub rsp, 32+8
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10

    ; Validate file handle
    test rcx, rcx
    jz .close_fail_invalid_handle
    
    ; Get the Close function from the file handle
    mov rbx, [rcx]       ; Get the file protocol interface
    test rbx, rbx
    jz .close_fail_invalid_handle
    
    mov r10, [rbx + OFFSET_FILE_CLOSE]
    test r10, r10
    jz .close_fail_no_func
    
    ; RCX already contains file handle
    call r10
    
    ; RAX holds status
    jmp .close_done

.close_fail_invalid_handle:
    mov rax, EFI_INVALID_PARAMETER
    jmp .close_done
    
.close_fail_no_func:
    mov rax, EFI_UNSUPPORTED
    
.close_done:
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiReadFile: Reads data from a file.
; Input: RCX = File handle
;        RDX = Buffer size (pointer to size variable, updated with bytes read)
;        R8  = Buffer to read into
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiReadFile:
    push rbp
    mov rbp, rsp
    sub rsp, 32+8
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10
    push r12
    push r13
    push r14

    ; Save input parameters
    mov r12, rcx         ; Save File handle
    mov r13, rdx         ; Save Buffer size pointer
    mov r14, r8          ; Save Buffer pointer
    
    ; Validate parameters
    test r12, r12
    jz .read_fail_invalid_handle
    
    test r13, r13
    jz .read_fail_invalid_size
    
    test r14, r14
    jz .read_fail_invalid_buffer
    
    ; Get the Read function from the file handle
    mov rbx, [r12]       ; Get the file protocol interface
    test rbx, rbx
    jz .read_fail_invalid_handle
    
    mov r10, [rbx + OFFSET_FILE_READ]
    test r10, r10
    jz .read_fail_no_func
    
    ; Prepare arguments for File->Read call
    mov rcx, r12         ; RCX = File handle
    mov rdx, r13         ; RDX = Buffer size pointer
    mov r8, r14          ; R8 = Buffer pointer
    
    ; Call File->Read
    call r10
    
    ; Check for errors
    test rax, rax
    jnz .read_done       ; If error, return it
    
    ; Success - return EFI_SUCCESS
    xor rax, rax
    jmp .read_done
    
.read_fail_invalid_handle:
.read_fail_invalid_size:
.read_fail_invalid_buffer:
    mov rax, EFI_INVALID_PARAMETER
    jmp .read_done
    
.read_fail_no_func:
    mov rax, EFI_UNSUPPORTED
    
.read_done:
    pop r14
    pop r13
    pop r12
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;--------------------------------------------------------------------------
; UefiWriteFile: Writes data to a file.
; Input: RCX = File handle
;        RDX = Buffer size (pointer to size variable, updated with bytes written)
;        R8  = Buffer to write from
; Output: RAX = Status
;--------------------------------------------------------------------------
UefiWriteFile:
    push rbp
    mov rbp, rsp
    sub rsp, 32+8
    and rsp, -16
    push rbx
    push rsi
    push rdi
    push r10
    push r12
    push r13
    push r14

    ; Save input parameters
    mov r12, rcx         ; Save File handle
    mov r13, rdx         ; Save Buffer size pointer
    mov r14, r8          ; Save Buffer pointer
    
    ; Validate parameters
    test r12, r12
    jz .write_fail_invalid_handle
    
    test r13, r13
    jz .write_fail_invalid_size
    
    test r14, r14
    jz .write_fail_invalid_buffer
    
    ; Get the Write function from the file handle
    mov rbx, [r12]       ; Get the file protocol interface
    test rbx, rbx
    jz .write_fail_invalid_handle
    
    mov r10, [rbx + OFFSET_FILE_WRITE]
    test r10, r10
    jz .write_fail_no_func
    
    ; Prepare arguments for File->Write call
    mov rcx, r12         ; RCX = File handle
    mov rdx, r13         ; RDX = Buffer size pointer
    mov r8, r14          ; R8 = Buffer pointer
    
    ; Call File->Write
    call r10
    
    ; Check for errors
    test rax, rax
    jnz .write_done      ; If error, return it
    
    ; Success - return EFI_SUCCESS
    xor rax, rax
    jmp .write_done
    
.write_fail_invalid_handle:
.write_fail_invalid_size:
.write_fail_invalid_buffer:
    mov rax, EFI_INVALID_PARAMETER
    jmp .write_done
    
.write_fail_no_func:
    mov rax, EFI_UNSUPPORTED
    
.write_done:
    pop r14
    pop r13
    pop r12
    pop r10
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

;; --- File: boot_defs_temp.inc ---
%ifndef BOOT_DEFS_TEMP_INC
%define BOOT_DEFS_TEMP_INC

; ===========================================
; --- Core System Definitions ---
; ===========================================
%define IDT_MAX_ENTRIES         256
%define KEY_BUFFER_SIZE         256
%define CODE64_SEL              0x18
%define DATA64_SEL              0x20

; --- Page Sizes ---
%define PAGE_SIZE_4K            4096
%define PAGE_SIZE_2M            (2 * 1024 * 1024)

; --- Page Table Entry Flags ---
%define PTE_PRESENT             (1 << 0)
%define PTE_WRITABLE            (1 << 1)
%define PTE_USER                (1 << 2)
%define PTE_WT                  (1 << 3)
%define PTE_CD                  (1 << 4)
%define PTE_ACCESSED            (1 << 5)
%define PTE_DIRTY               (1 << 6)
%define PTE_PS                  (1 << 7)  ; Page Size (1 for 2MB/1GB page)
%define PTE_GLOBAL              (1 << 8)
%define PTE_XD                  (1 << 63) ; Execute Disable

; --- Standard PTE Flags ---
%define KERNEL_PTE_FLAGS        (PTE_PRESENT | PTE_WRITABLE)
%define DATA_PTE_FLAGS          (PTE_PRESENT | PTE_WRITABLE | PTE_XD)

; ===========================================
; --- Legacy Hardware Definitions ---
; ===========================================
%define PIC1_COMMAND            0x20
%define PIC1_DATA               0x21
%define PIC2_COMMAND            0xA0
%define PIC2_DATA               0xA1
%define PIC_EOI                 0x20
%define PIC1_IRQ_START          0x20 ; Default base vector for IRQs 0-7
%define PIC2_IRQ_START          0x28 ; Default base vector for IRQs 8-15
%define IRQ_VECTOR_BASE         0x20 ; General base for hardware IRQs (decimal 32)

%define KB_DATA_PORT            0x60
%define KB_STATUS_PORT          0x64
%define KB_IRQ                  1    ; Legacy IRQ number for keyboard
%define SC_LSHIFT_MAKE          0x2A
%define SC_LSHIFT_BREAK         0xAA
%define SC_RSHIFT_MAKE          0x36
%define SC_RSHIFT_BREAK         0xB6
%define SC_CAPSLOCK_MAKE        0x3A

%define PCI_CONFIG_ADDRESS      0xCF8
%define PCI_CONFIG_DATA         0xCFC
%define PCI_CLASS_MASS_STORAGE  0x01
%define PCI_SUBCLASS_SATA       0x06
%define PCI_PROGIF_AHCI         0x01

; ===========================================
; --- FAT32 Driver Definitions ---
; ===========================================
%define FAT_ERR_OK              0
%define FAT_ERR_NOT_INIT        1
%define FAT_ERR_DISK_ERROR      2
%define FAT_ERR_NOT_FOUND       3
%define FAT_ERR_BAD_CLUSTER     4
%define FAT_ERR_FAT_READ        5
%define FAT_ERR_EOF             6
%define FAT_ERR_BUFFER_SMALL    7
%define FAT_ERR_NOT_FAT32       8
%define FAT_ERR_BAD_BPB         9
%define FAT_ERR_INVALID_PARAM   10
%define FAT_ERR_NO_FREE_CLUSTER 11
%define FAT_ERR_NOT_IMPLEMENTED 12
%define FAT_ERR_FILE_EXISTS     13
%define FAT_ERR_DIR_FULL        14
%define FAT_ERR_GPT_ERROR       15

%define FAT_BPB_BytesPerSector    11
%define FAT_BPB_SectorsPerCluster 13
%define FAT_BPB_ReservedSectors   14
%define FAT_BPB_NumberOfFATs      16
%define FAT32_BPB_SectorsPerFAT32 36
%define FAT32_BPB_RootCluster     44
%define FAT32_BPB_FSInfo          48
%define FAT32_BPB_FSType          82

%define FAT_DIRENT_Name         0
%define FAT_DIRENT_Attributes   11
%define FAT_DIRENT_NTRes        12
%define FAT_DIRENT_CrtTimeTenth 13
%define FAT_DIRENT_CrtTime      14
%define FAT_DIRENT_CrtDate      16
%define FAT_DIRENT_LstAccDate   18
%define FAT_DIRENT_FstClusHI    20
%define FAT_DIRENT_WrtTime      22
%define FAT_DIRENT_WrtDate      24
%define FAT_DIRENT_FstClusLO    26
%define FAT_DIRENT_FileSize     28
%define FAT_DIRENT_SIZE         32

%define FAT_ATTR_READ_ONLY      0x01
%define FAT_ATTR_HIDDEN         0x02
%define FAT_ATTR_SYSTEM         0x04
%define FAT_ATTR_VOLUME_ID      0x08
%define FAT_ATTR_DIRECTORY      0x10
%define FAT_ATTR_ARCHIVE        0x20
%define FAT_ATTR_LONG_NAME      0x0F
%define FAT_ATTR_LFN_MASK       0x3F

%define FAT32_CLUSTER_FREE      0x00000000
%define FAT32_CLUSTER_RESERVED  0x00000001
%define FAT32_CLUSTER_MIN_VALID 0x00000002
%define FAT32_CLUSTER_MAX_VALID 0x0FFFFFF6
%define FAT32_CLUSTER_BAD       0x0FFFFFF7
%define FAT32_CLUSTER_EOF_MIN   0x0FFFFFF8
%define FAT32_CLUSTER_EOF_MAX   0x0FFFFFFF

; ===========================================
; --- GPT (GUID Partition Table) Definitions ---
; ===========================================
%define GPT_PMBR_LBA              0
%define GPT_PRIMARY_HEADER_LBA    1

%define GPT_HEADER_SIGNATURE_QWORD 0x5452415020494645 ; "EFI PART"

struc GPT_HEADER
    .Signature              resq 1
    .Revision               resd 1
    .HeaderSize             resd 1
    .HeaderCRC32            resd 1
    .Reserved               resd 1
    .MyLBA                  resq 1
    .AlternateLBA           resq 1
    .FirstUsableLBA         resq 1
    .LastUsableLBA          resq 1
    .DiskGUID               resb 16
    .PartitionEntryLBA      resq 1
    .NumberOfPartitionEntries resd 1
    .SizeOfPartitionEntry   resd 1
    .PartitionEntryArrayCRC32 resd 1
endstruc
%define GPT_HEADER_SIZE_EXPECTED 92

struc GPT_PARTITION_ENTRY
    .PartitionTypeGUID      resb 16
    .UniquePartitionGUID    resb 16
    .StartingLBA            resq 1
    .EndingLBA              resq 1
    .Attributes             resq 1
    .PartitionName          resw 36 ; UTF-16LE, 72 bytes
endstruc
%define GPT_PARTITION_ENTRY_SIZE_EXPECTED 128

EFI_SYSTEM_PARTITION_GUID:
    dd 0xC12A7328
    dw 0xF81F, 0x11D2
    db 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
MS_BASIC_DATA_PARTITION_GUID:
    dd 0xEBD0A0A2
    dw 0xB9E5, 0x4433
    db 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7

%define CRC32_POLYNOMIAL 0xEDB88320 ; For CRC32 calculations

%define GPT_ERR_OK                    0
%define GPT_ERR_AHCI_READ_FAIL        1
%define GPT_ERR_PMBR_INVALID          2
%define GPT_ERR_HEADER_SIG_INVALID    3
%define GPT_ERR_HEADER_SIZE_INVALID   4
%define GPT_ERR_HEADER_CRC_INVALID    5
%define GPT_ERR_PART_ARRAY_CRC_INVALID 6
%define GPT_ERR_NO_FAT32_FOUND        7
%define GPT_ERR_BUFFER_TOO_SMALL      8
%define GPT_ERR_INVALID_PARAMS        9

; ===========================================
; --- UEFI Specific Definitions ---
; ===========================================
%define EFI_SUCCESS             0
%define EFI_LOAD_ERROR          0x8000000000000001
%define EFI_INVALID_PARAMETER   0x8000000000000002
%define EFI_UNSUPPORTED         0x8000000000000003
%define EFI_BAD_BUFFER_SIZE     0x8000000000000004
%define EFI_BUFFER_TOO_SMALL    0x8000000000000005
%define EFI_NOT_READY           0x8000000000000006
%define EFI_DEVICE_ERROR        0x8000000000000007
%define EFI_WRITE_PROTECTED     0x8000000000000008
%define EFI_OUT_OF_RESOURCES    0x8000000000000009
%define EFI_NOT_FOUND           0x800000000000000E

%define EFI_SYSTEM_TABLE_SIGNATURE  0x5453595320494249 ; "IBI SYST"
%define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544f4f42 ; "BOOTSERV"

; Memory Types
%define EfiReservedMemoryType       0
%define EfiLoaderCode               1
%define EfiLoaderData               2
%define EfiBootServicesCode         3
%define EfiBootServicesData         4
%define EfiRuntimeServicesCode      5
%define EfiRuntimeServicesData      6
%define EfiConventionalMemory       7
%define EfiUnusableMemory           8
%define EfiACPIReclaimMemory        9
%define EfiACPIMemoryNVS            10
%define EfiMemoryMappedIO           11
%define EfiMemoryMappedIOPortSpace  12
%define EfiPalCode                  13
%define EfiPersistentMemory         14
%define EfiMaxMemoryType            15

; AllocateType for AllocatePages
%define AllocateAnyPages            0
%define AllocateMaxAddress          1
%define AllocateAddress             2

struc EFI_MEMORY_DESCRIPTOR
    .Type           resd 1  ; UINT32
    .PhysicalStart  resq 1  ; EFI_PHYSICAL_ADDRESS (Padding is implicit if DescriptorSize indicates it)
    .VirtualStart   resq 1  ; EFI_VIRTUAL_ADDRESS
    .NumberOfPages  resq 1  ; UINT64
    .Attribute      resq 1  ; UINT64
endstruc
; Common offsets assuming DescriptorSize is 40 (Type(4)+Padding(4)+Phys(8)+Virt(8)+Pages(8)+Attr(8))
; If DescriptorSize from GetMemoryMap is 36, then padding is not there.
%define OFFSET_MD_TYPE            0
%define OFFSET_MD_PHYSICALSTART   8 ; Assuming 4-byte padding after Type field
%define OFFSET_MD_NUMBEROFPAGES  24 ; Assuming 4-byte padding
%define OFFSET_MD_ATTRIBUTE      32 ; Assuming 4-byte padding

; GUIDs
EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID: dd 0x9042A9DE; dw 0x23DC, 0x4A38; db 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A
EFI_LOADED_IMAGE_PROTOCOL_GUID:    dd 0x5B1B31A1; dw 0x9562, 0x11D2; db 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID: dd 0x964E5B22; dw 0x6459, 0x11D2; db 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B

; Offsets in EFI_SYSTEM_TABLE (verify with specific UEFI spec version)
%define OFFSET_ST_HDR_SIGNATURE     0x00 ; Table Header part
%define OFFSET_ST_HDR_REVISION      0x08
%define OFFSET_ST_HDR_SIZE          0x0C
%define OFFSET_ST_HDR_CRC32         0x10
%define OFFSET_ST_HDR_RESERVED      0x14
%define OFFSET_ST_FIRMWAREVENDOR    0x18
%define OFFSET_ST_FIRMWAREREVISION  0x20
%define OFFSET_ST_CONSOLEINHANDLE   0x24
%define OFFSET_ST_CONIN             0x28
%define OFFSET_ST_CONSOLEOUTHANDLE  0x30
%define OFFSET_ST_CONOUT            0x38 ; EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConsoleOut
%define OFFSET_ST_STANDARDERRORHANDLE 0x40
%define OFFSET_ST_STDERR            0x48
%define OFFSET_ST_RUNTIMESERVICES   0x50
%define OFFSET_ST_BOOTSERVICES      0x58 ; EFI_BOOT_SERVICES *BootServices
%define OFFSET_ST_NUMBEROFTABLEENTRIES 0x60
%define OFFSET_ST_CONFIGURATIONTABLE 0x68

; Offsets in EFI_BOOT_SERVICES (verify with specific UEFI spec version)
%define OFFSET_BS_HDR_SIGNATURE     0x00
%define OFFSET_BS_HDR_REVISION      0x08
%define OFFSET_BS_HDR_SIZE          0x0C
%define OFFSET_BS_HDR_CRC32         0x10
%define OFFSET_BS_HDR_RESERVED      0x14
; Function pointers from BS base:
%define OFFSET_BS_RAISE_TPL         0x18 ; Index 0*8 relative to functions
%define OFFSET_BS_RESTORE_TPL       0x20 ; Index 1*8
%define OFFSET_BS_ALLOCATEPAGES     0x28 ; Index 2*8 -> EFI_ALLOCATE_PAGES AllocatePages;
%define OFFSET_BS_FREEPAGES         0x30 ; Index 3*8 -> EFI_FREE_PAGES FreePages;
%define OFFSET_BS_GETMEMORYMAP      0x38 ; Index 4*8 -> EFI_GET_MEMORY_MAP GetMemoryMap;
%define OFFSET_BS_ALLOCATEPOOL      0x40 ; Index 5*8
%define OFFSET_BS_FREEPOOL          0x48 ; Index 6*8
%define OFFSET_BS_CREATEEVENT       0x50 ; Index 7*8
%define OFFSET_BS_SETTIMER          0x58 ; Index 8*8
%define OFFSET_BS_WAITFOREVENT      0x60 ; Index 9*8
%define OFFSET_BS_SIGNALEVENT       0x68 ; Index 10*8
%define OFFSET_BS_CLOSEEVENT        0x70 ; Index 11*8
%define OFFSET_BS_CHECKEVENT        0x78 ; Index 12*8
%define OFFSET_BS_INSTALLPROTOCOLINTERFACE 0x80 ; Index 13*8
%define OFFSET_BS_REINSTALLPROTOCOLINTERFACE 0x88 ; Index 14*8
%define OFFSET_BS_UNINSTALLPROTOCOLINTERFACE 0x90 ; Index 15*8
%define OFFSET_BS_HANDLEPROTOCOL    0x98 ; Index 16*8 -> EFI_HANDLE_PROTOCOL HandleProtocol;
%define OFFSET_BS_REGISTERPROTOCOLNOTIFY 0xA8 ; Index 18*8
%define OFFSET_BS_LOCATEHANDLE      0xB0 ; Index 19*8
%define OFFSET_BS_LOCATEDEVICEPATH  0xB8 ; Index 20*8
%define OFFSET_BS_INSTALLCONFIGURATIONTABLE 0xC0 ; Index 21*8
%define OFFSET_BS_LOADIMAGE         0xC8 ; Index 22*8
%define OFFSET_BS_STARTIMAGE        0xD0 ; Index 23*8
%define OFFSET_BS_EXIT              0xD8 ; Index 24*8
%define OFFSET_BS_UNLOADIMAGE       0xE0 ; Index 25*8
%define OFFSET_BS_EXITBOOTSERVICES  0xE8 ; Index 26*8 -> EFI_EXIT_BOOT_SERVICES ExitBootServices;
%define OFFSET_BS_GETNEXTMONOTONICCOUNT 0xF0 ; Index 27*8
%define OFFSET_BS_STALL             0xF8 ; Index 28*8
%define OFFSET_BS_SETWATCHDOGTIMER  0x100; Index 29*8
%define OFFSET_BS_CONNECTCONTROLLER 0x108; Index 30*8
%define OFFSET_BS_DISCONNECTCONTROLLER 0x110; Index 31*8
%define OFFSET_BS_OPENPROTOCOL      0x118; Index 32*8 -> EFI_OPEN_PROTOCOL OpenProtocol;
%define OFFSET_BS_CLOSEPROTOCOL     0x120; Index 33*8 -> EFI_CLOSE_PROTOCOL CloseProtocol;
%define OFFSET_BS_OPENPROTOCOLINFORMATION 0x128; Index 34*8
%define OFFSET_BS_PROTOCOLSPERHANDLE 0x130; Index 35*8
%define OFFSET_BS_LOCATEHANDLEBUFFER 0x138; Index 36*8
%define OFFSET_BS_LOCATEPROTOCOL    0x140; Index 37*8 -> EFI_LOCATE_PROTOCOL LocateProtocol;
%define OFFSET_BS_INSTALLMULTIPLEPROTOCOLINTERFACES 0x148; Index 38*8
%define OFFSET_BS_UNINSTALLMULTIPLEPROTOCOLINTERFACES 0x150; Index 39*8
%define OFFSET_BS_CALCULATECR32     0x158; Index 40*8
%define OFFSET_BS_COPYMEM           0x160; Index 41*8
%define OFFSET_BS_SETMEM            0x168; Index 42*8
%define OFFSET_BS_CREATEEVENTEX     0x170; Index 43*8

; Offsets in EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
%define OFFSET_CONOUT_RESET         0x00
%define OFFSET_CONOUT_OUTPUTSTRING  0x08
%define OFFSET_CONOUT_TESTSTRING    0x10
%define OFFSET_CONOUT_QUERYMODE     0x18
%define OFFSET_CONOUT_SETMODE       0x20
%define OFFSET_CONOUT_SETATTRIBUTE  0x28
%define OFFSET_CONOUT_CLEARSCREEN   0x30
%define OFFSET_CONOUT_SETCURSORPOS  0x38
%define OFFSET_CONOUT_ENABLECURSOR  0x40
%define OFFSET_CONOUT_MODE          0x48 ; Pointer to EFI_SIMPLE_TEXT_OUTPUT_MODE

; Offsets in EFI_GRAPHICS_OUTPUT_PROTOCOL and related structures
%define OFFSET_GOP_QUERYMODE       0x08
%define OFFSET_GOP_SETMODE         0x10
%define OFFSET_GOP_BLT             0x18
%define OFFSET_GOP_MODE            0x20
%define OFFSET_GOP_MODE_MAXMODE    0x00
%define OFFSET_GOP_MODE_MODE       0x04
%define OFFSET_GOP_MODE_INFO       0x08
%define OFFSET_GOP_MODE_FBBASE     0x18
%define OFFSET_GOP_MODE_FBSIZE     0x20
%define OFFSET_GOP_INFO_HRES       0x04
%define OFFSET_GOP_INFO_VRES       0x08
%define OFFSET_GOP_INFO_PIXELFMT   0x0C
%define OFFSET_GOP_INFO_PIXELSPERSCANLINE 0x14
%define PixelBlueGreenRedReserved8BitPerColor 1

; Offsets in EFI_LOADED_IMAGE_PROTOCOL
%define OFFSET_LOADED_IMAGE_IMAGEBASE   0x40
%define OFFSET_LOADED_IMAGE_IMAGESIZE   0x48

; EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
%define OFFSET_SFS_REVISION       0x00
%define OFFSET_SFS_OPENVOLUME     0x08

; EFI_FILE_PROTOCOL
%define OFFSET_FP_REVISION        0x00
%define OFFSET_FP_OPEN            0x08
%define OFFSET_FP_CLOSE           0x10
%define OFFSET_FP_READ            0x20

; EFI_FILE_PROTOCOL Open() Modes
%define EFI_FILE_MODE_READ        0x0000000000000001
%define EFI_FILE_MODE_WRITE       0x0000000000000002
%define EFI_FILE_MODE_CREATE      0x8000000000000000

; ===========================================
; --- AHCI Definitions ---
; ===========================================
%define AHCI_CAP        0x00
%define AHCI_GHC        0x04
%define AHCI_IS         0x08
%define AHCI_PI         0x0C
%define AHCI_PxCLB      0x00
%define AHCI_PxCLBU     0x04
%define AHCI_PxFB       0x08
%define AHCI_PxFBU      0x0C
%define AHCI_PxIS       0x10
%define AHCI_PxIE       0x14
%define AHCI_PxCMD      0x18
%define AHCI_PxTFD      0x20
%define AHCI_PxSIG      0x24
%define AHCI_PxSSTS     0x28
%define AHCI_PxSCTL     0x2C
%define AHCI_PxSERR     0x30
%define AHCI_PxSACT     0x34
%define AHCI_PxCI       0x38
%define AHCI_GHC_AE     (1 << 31)
%define AHCI_GHC_IE     (1 << 1)
%define AHCI_PxCMD_ST   (1 << 0)
%define AHCI_PxCMD_FRE  (1 << 4)
%define AHCI_PxCMD_FR   (1 << 14)
%define AHCI_PxCMD_CR   (1 << 15)
%define AHCI_PxIS_TFES  (1 << 30)
%define AHCI_PxIS_DHRS  (1 << 0)
%define AHCI_PxIS_SDBS  (1 << 3)
%define AHCI_PxIS_DPS   (1 << 5)
%define AHCI_PxIE_TFEE  (1 << 30)
%define AHCI_PxIE_DHRE  (1 << 0)
%define AHCI_PxIE_SDBE  (1 << 3)
%define AHCI_PxIE_DPE   (1 << 5)

struc AHCI_COMMAND_HEADER
    .Flags      resw 1
    .PRDTL      resw 1
    .PRDBC      resd 1
    .CTBA       resd 1
    .CTBAU      resd 1
    .Reserved   resd 4
endstruc
%define AHCI_COMMAND_HEADER_SIZE 32
%define AHCI_CH_FLAGS_CFL_MASK  0x001F
%define AHCI_CH_FLAGS_WRITE     (1 << 6)
%define AHCI_CH_FLAGS_PRDTL_SHIFT 16

struc AHCI_PRDT_ENTRY
    .DBA        resd 1
    .DBAU       resd 1
    .Reserved   resd 1
    .DBC        resd 1
endstruc
%define AHCI_PRDT_ENTRY_SIZE 16
%define AHCI_PRDT_DBC_I         (1 << 31) ; Interrupt on completion bit in DBC

%define AHCI_FIS_REG_H2D_SIZE 20
%define AHCI_FIS_TYPE_REG_H2D   0x27
%define AHCI_FIS_TYPE           0x00
%define AHCI_FIS_PM_PORT        0x01
%define AHCI_FIS_COMMAND        0x02
%define AHCI_FIS_LBA0           0x04
%define AHCI_FIS_LBA1           0x05
%define AHCI_FIS_LBA2           0x06
%define AHCI_FIS_DEVICE         0x07
%define AHCI_FIS_LBA3           0x08
%define AHCI_FIS_LBA4           0x09
%define AHCI_FIS_LBA5           0x0A
%define AHCI_FIS_COUNT_L        0x0C
%define AHCI_FIS_COUNT_H        0x0D

%define ATA_CMD_READ_DMA_EXT    0x25
%define ATA_CMD_WRITE_DMA_EXT   0x35
%define ATA_CMD_CACHE_FLUSH_EXT 0xEA

%define AHCI_CMD_SLOTS          32
%define AHCI_CMD_LIST_SIZE      (AHCI_CMD_SLOTS * AHCI_COMMAND_HEADER_SIZE)
%define AHCI_FIS_RX_SIZE        256
%define AHCI_CMD_TABLE_SIZE     8192
%define AHCI_PORT_REGS_SIZE     0x80
%define AHCI_CT_CFIS_OFFSET     0x00
%define AHCI_CT_PRDT_OFFSET     0x80
%define AHCI_IRQ_FLAG_COMPLETE  (1 << 0)
%define AHCI_IRQ_FLAG_ERROR     (1 << 1)
%define AHCI_CMD_TIMEOUT_LOOPS  500000000

; ===========================================
; --- APIC Definitions ---
; ===========================================
%define LAPIC_REG_ID            0x020
%define LAPIC_REG_EOI           0x0B0
%define LAPIC_REG_SPURIOUS      0x0F0
%define LAPIC_REG_TPR           0x080
%define LAPIC_SPURIOUS_ENABLE   (1 << 8)
%define MSR_APIC_BASE           0x1B
%define MSR_APIC_BASE_ENABLE    (1 << 11)
%define IOAPIC_INDEX_REG        0x00
%define IOAPIC_DATA_REG         0x10
%define IOAPIC_REG_VER          0x01
%define IOAPIC_REG_REDTBL_BASE  0x10
%define RTE_MASKED              (1 << 16)
%define RTE_TRIGGER_MODE_EDGE   (0 << 15)
%define RTE_TRIGGER_MODE_LEVEL  (1 << 15)
%define RTE_INT_POLARITY_HIGH   (0 << 13)
%define RTE_INT_POLARITY_LOW    (1 << 13)
%define RTE_DELIVERY_MODE_FIXED (0 << 8)
%define RTE_DESTINATION_SHIFT   56

; ===========================================
; --- ACPI Definitions ---
; ===========================================
%define RSDP_SIGNATURE_QWORD   0x2052545020445352 ; "RSD PTR "
%define BDA_EBDA_POINTER       0x040E
%define EBDA_SEARCH_START      0x80000
%define EBDA_SEARCH_END        0xA0000
%define BIOS_ROM_SEARCH_START  0xE0000
%define BIOS_ROM_SEARCH_END    0xFFFFF
%define RSDP_SEARCH_ALIGNMENT  16

struc RSDP_DESCRIPTOR_V1
    .Signature          resq 1
    .Checksum           resb 1
    .OEMID              resb 6
    .Revision           resb 1
    .RsdtAddress        resd 1
endstruc
%define RSDP_DESCRIPTOR_V1_SIZE 20

struc RSDP_DESCRIPTOR_V2
    .Signature_v2       resq 1 ; V1 Part
    .Checksum_v2        resb 1
    .OEMID_v2           resb 6
    .Revision_v2        resb 1
    .RsdtAddress_v2     resd 1
    .Length             resd 1 ; V2 Part
    .XsdtAddress        resq 1
    .ExtendedChecksum   resb 1
    .Reserved           resb 3
endstruc
%define RSDP_DESCRIPTOR_V2_SIZE 36

struc ACPI_TABLE_HEADER
    .Signature          resd 1
    .Length             resd 1
    .Revision           resb 1
    .Checksum           resb 1
    .OEMID              resb 6
    .OEMTableID         resq 1 ; Often char[8]
    .OEMRevision        resd 1
    .CreatorID          resd 1
    .CreatorRevision    resd 1
endstruc
%define ACPI_TABLE_HEADER_SIZE 36

%define ACPI_SIG_RSDT   0x54445352  ; "RSDT"
%define ACPI_SIG_XSDT   0x54445358  ; "XSDT"
%define ACPI_SIG_MADT   0x43495041  ; "APIC" (MADT)
%define ACPI_SIG_SRAT   0x54415253  ; "SRAT"
%define ACPI_SIG_MCFG   0x4746434D  ; "MCFG"

; MADT (Multiple APIC Description Table)
struc ACPI_MADT_TABLE_HEADER ; Specific part after common ACPI_TABLE_HEADER
    .LocalApicAddress   resd 1
    .Flags              resd 1  ; Bit 0: PCAT_COMPAT
endstruc
%define ACPI_MADT_TABLE_HEADER_SIZE 8

struc ACPI_MADT_ENTRY_HEADER
    .Type               resb 1
    .Length             resb 1
endstruc
%define ACPI_MADT_ENTRY_HEADER_SIZE 2

%define ACPI_MADT_TYPE_LOCAL_APIC         0
%define ACPI_MADT_TYPE_IO_APIC            1
%define ACPI_MADT_TYPE_INTERRUPT_OVERRIDE 2

struc ACPI_MADT_LOCAL_APIC ; Type 0
    .AcpiProcessorId    resb 1
    .ApicId             resb 1
    .Flags              resd 1
endstruc

struc ACPI_MADT_IO_APIC ; Type 1
    .IoApicId           resb 1
    .Reserved           resb 1
    .IoApicAddress      resd 1
    .GlobalSystemInterruptBase resd 1
endstruc

struc ACPI_MADT_INT_OVERRIDE ; Type 2
    .Bus                resb 1 ; 0 for ISA
    .Source             resb 1 ; ISA IRQ
    .GlobalSystemInterrupt resd 1 ; GSI
    .Flags              resw 1 ; MPS INTI flags
endstruc

; SRAT (System Resource Affinity Table)
%define ACPI_SRAT_TYPE_PROCESSOR_AFFINITY  0
%define ACPI_SRAT_TYPE_MEMORY_AFFINITY     1

struc ACPI_SRAT_MEMORY_AFFINITY ; Type 1
    .ProximityDomain      resd 1
    .Reserved1            resw 1 ; Should be 0
    .BaseAddressLow       resd 1
    .BaseAddressHigh      resd 1
    .LengthLow            resd 1
    .LengthHigh           resd 1
    .Reserved2            resd 1
    .Flags                resd 1  ; Bit 0: Enabled
    .Reserved3            resq 1
endstruc
%define SRAT_MEM_ENABLED        (1 << 0)

; MCFG (Memory Mapped Configuration Space Base Address Description Table)
struc ACPI_MCFG_TABLE_HEADER ; Specific part
    .Reserved           resq 1 ; 8 bytes
endstruc

struc ACPI_MCFG_ALLOCATION_STRUCTURE
    .BaseAddress        resq 1
    .PCISegmentGroup    resw 1
    .StartPCIBusNumber  resb 1
    .EndPCIBusNumber    resb 1
    .Reserved           resd 1
endstruc
%define ACPI_MCFG_ALLOCATION_STRUCTURE_SIZE 16

%endif ; BOOT_DEFS_TEMP_INC
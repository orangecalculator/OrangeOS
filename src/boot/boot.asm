; Origin is set to 0x7c00 by the linker script.
BITS 16

EXTERN kernel_start

SECTION .boot

  ; Actually, most of the instructions are position independent,
  ; so the origin settings will not likely matter.

  ; At this point, this binary is loaded into 0X7C00 and executed in real mode.
  ; See also: https://www.glamenv-septzen.net/en/view/6
  ; See also: IBM Personal Computer Hardware Reference Library, Technical Reference
  ; See also: Ralf Brown's interrupt list, INT 19 - SYSTEM - BOOTSTRAP LOADER
  ;
  ; Note that the stack is not guaranteed to point to a suitable location.
  ; See also: https://retrocomputing.stackexchange.com/questions/2927/did-the-intel-8086-8088-not-guarantee-the-value-of-sssp-immediately-after-reset
  ;
  ; Actually, nothing is standardized or guaranteed about
  ; the processor state after BIOS initialization.
  ; The only reliable value that can be leveraged are the followings.
  ; - CS:IP points to 0x07c00, but can vary between 7c0:0000 and 000:7c00,
  ;   or any value pair that points to 0x7c00.
  ; - DL holds the drive number where the BIOS loaded the first sector from.
  ; See also: https://forum.osdev.org/viewtopic.php?t=30857
start:

.bios_parameter_block:
db 0xEB, 0x3C, 0x90 ; JMP SHORT 3C NOP
times 2 + 0x3C - ($ - $$) db 0

  ; Setup segment registers for consistency
  ; Cannot use the stack until setup is complete
  ; Note that the stack is initialized to a suitable place,
  ; but does not seem to be in standard protocol
  jmp 0x0000:.start_context_setup
.start_context_setup:
  cli
  mov ax, 0x0000
  mov ds, ax
  mov es, ax
  mov ax, 0x0000
  mov ss, ax
  mov sp, 0x7c00
  sti
.finish_context_setup:
  jmp protected_mode_bootstrap

printstr:
  mov bx, 0
.call_printchar:
  lodsb
  cmp al, 0
  je .finish_printchar
  call printchar
  jmp .call_printchar
.finish_printchar:
  ret

; Arguments: character in al, cursor in bx
printchar:
  mov ah, 0x0e
  int 0x10
  ret

; Note that even though you should use 16 bit instructions in real mode, you can override to use 32 bit instructions for a single instruction.
; See also: https://stackoverflow.com/questions/6917503/is-it-possible-to-use-32-bits-registers-instructions-in-real-mode
protected_mode_bootstrap:
  cli
  lgdt[gdt_descriptor]
  mov eax, cr0
  or eax, 0x1
  mov cr0, eax
  ; sti ; TODO: Why do you need to disable interrupt here?

%define CODE_SEG gdt_table_entry_code - gdt_table_start
%define DATA_SEG gdt_table_entry_data - gdt_table_start

  ; Jump is required to start protected mode functionality
  jmp CODE_SEG:protected_mode_start

BITS 32

protected_mode_start:

.setup_data_segment_selector:
  ; Setup data segment selectors here
  mov ax, DATA_SEG
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

EXTERN KERNEL_RUNTIME_START
EXTERN DISK_KERNEL_ADDR_SECTOR
EXTERN KERNEL_RUNTIME_SIZE_SECTOR

.load_kernel:
  mov edi, KERNEL_RUNTIME_START
  mov esi, DISK_KERNEL_ADDR_SECTOR
  mov ecx, KERNEL_RUNTIME_SIZE_SECTOR
  call ata_lba_read_simple

.kernel_trampoline:
  jmp kernel_start

; void ata_lba_read_simple([edi] void *buffer, [esi] unsigned long diskaddr, [ecx] uint8_t nsector)
ata_lba_read_simple:
  ; Send lba[24:32]
  mov edx, 0x01f6
  mov eax, esi
  shr eax, 24
  or al, 0xe0
  out dx, al

  ; Send number of sectors to read
  mov edx, 0x01f2
  mov eax, ecx
  out dx, al

  ; Send lba[0:8]
  mov edx, 0x1f3
  mov eax, esi
  out dx, al

  ; Send lba[8:16]
  mov edx, 0x1f4
  mov eax, esi
  shr eax, 8
  out dx, al

  ; Send lba[16:24]
  mov edx, 0x1f5
  mov eax, esi
  shr eax, 16
  out dx, al

  ; Initiate read operation
  mov edx, 0x1f7
  mov al, 0x20
  out dx, al

.wait_ready:
  in al, dx
  test al, 0x08
  jz .wait_ready

.read_sector:
  mov edx, 0x1f0
  shl ecx, 8 ; Read in word graularity, so multiply by 256
  rep insw

  ret

BITS 16

str_disk_success:
  db "DISK READ SUCCESS", 0
str_disk_error:
  db "DISK READ ERROR", 0

; GDT Descriptor
times 480-($-$$) db 0

gdt_table_start:
gdt_table_entry_null:
  dd 0x00000000
  dd 0x00000000

gdt_table_entry_code:
  dw 0xffff, 0x0000
  db 0x00, 0x9b, 0xcf, 0x00

gdt_table_entry_data:
  dw 0xffff, 0x0000
  db 0x00, 0x93, 0xcf, 0x00
gdt_table_end:

gdt_descriptor:
  dw gdt_table_end - gdt_table_start - 1 ; Subtracted by 1 due to architecture design
  dd gdt_table_start

; Boot signature
times 510-($-$$) db 0
db 0x55
db 0xAA

end_of_bootsector:
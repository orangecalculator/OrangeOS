ORG 0
BITS 16

%define BASE_ADDR 0x7c00

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
  jmp 0x07c0:.start_context_setup
.start_context_setup:
  cli
  mov ax, 0x07c0
  mov ds, ax
  mov es, ax
  mov ax, 0x0000
  mov ss, ax
  mov sp, 0x7c00
  sti
.finish_context_setup:

  ; On entry, the number of current boot disk is provided on dl

  mov ah, 0x02
  mov al, 1
  mov ch, 0x00
  mov cl, 0x02 ; Second sector
  mov dh, 0x00
  ; dl is already set after BIOS initialization
  mov bx, 0x0200
  int 0x13

  jc .disk_read_error
.disk_read_success:
  mov si, str_disk_success
  call printstr
  mov al, ':'
  call printchar
  mov al, ' '
  call printchar
  mov si, end_of_bootsector
  call printstr
  jmp .disk_read_finish
.disk_read_error:
  mov si, str_disk_error
  call printstr
.disk_read_finish:

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

  jmp CODE_SEG:(BASE_ADDR + protected_mode_start)

BITS 32

protected_mode_start:

%define STACK_BASE_ADDR 0x00200000 ; 2M

.setup_data_segment_selector:
  ; Setup data segment selectors here
  mov ax, DATA_SEG
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  mov ebp, STACK_BASE_ADDR
  mov esp, ebp

.enable_a20_line:
  ; Enable the A20 line
  in al, 0x92
  or al, 0x02
  out 0x92, al

  jmp $

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
  dd BASE_ADDR + gdt_table_start

; Boot signature
times 510-($-$$) db 0
db 0x55
db 0xAA

end_of_bootsector:
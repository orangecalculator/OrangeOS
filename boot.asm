ORG 0
BITS 16
  ; Actually, most of the instructions are position independent,
  ; so the origin settings will not likely matter.

  ; At this point, this binary is loaded into 0X7C00
  ; See also: https://www.glamenv-septzen.net/en/view/6
  ; See also: IBM Personal Computer Hardware Reference Library, Technical Reference
  ; See also: Ralf Brown's interrupt list, INT 19 - SYSTEM - BOOTSTRAP LOADER
start:

.bios_parameter_block:
db 0xEB, 0x3C, 0x90 ; JMP SHORT 3C NOP
times 2 + 0x3C - ($ - $$) db 0

  ; Show register values on startup
  call printregs

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

  pusha

  ; Show register values after custom context setup
  call printregs

  ; Print a hello message
  mov si, hellomessage
  call printstroneline

  ; Dump the first sector contents in memory
  mov di, 0
  mov si, 0x100
  call dumpmemory

  popa

  ; On entry, the number of current boot disk is provided on dx
  ; TODO: load the bootloader from disk

  jmp $

handle_div_zero:
  pusha
  mov si, .div_zero_message
  call printstr
  popa
  iret

.div_zero_message:
  db "DIVZERO", 0

printregs:
  ; Here, we assume that the sp is setup to a legitimate address
  ; Print order is: CS, DS, ES, SS, FS, GS, DI, SI, BP, SP, BX, DX, CX, AX
  ; Register values will be preserved across calls,
  ; but a certain part of the stack will be tainted.
  ; There is actually nothing we can do about that.

  pusha
  push gs
  push fs
  push ss
  push es
  push ds
  push cs

  mov di, 0
.start_dump_reg:
  cmp di, 14
  jge .finish_dump_reg

  mov ax, di
  imul ax, 2
  mov si, ax
  add si, sp
  mov ax, word[ss:si]
  call printhex
  mov al, ' '
  call printchar

  add di, 1
  jmp .start_dump_reg
.finish_dump_reg:

  ; To complete a full line
  mov di, 0
.start_print_leftovers:
  cmp di, 10
  jge .finish_print_leftovers

  mov al, ' '
  call printchar

  add di, 1
  jmp .start_print_leftovers
.finish_print_leftovers:

  pop ax
  pop ax
  pop ax
  pop ax
  pop ax
  pop ax
  popa
  ret

dumpmemory:

.dump_start:
  cmp di, si
  jge .dump_finish

  ; We assume the address of dumped region is full address,
  ; so we use ss to use segment register with value 0.
  mov ax, word[ss:di]
  call printhex

  mov al, ' '
  call printchar

  add di, 2
  jmp .dump_start

.dump_finish:
  ret

printstroneline:
  push si
  call printstr
  pop ax
  sub si, ax
  mov ax, 80 + 1 ; si points next to the first null byte
  sub ax, si

  mov di, ax
  mov al, ' '
  call printcharntimes
  ret

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

printhex:
  push si
  mov bx, 0
  mov si, ax
  shr ax, 12
  and ax, 0xf
  call printhexchar
  mov ax, si
  shr ax, 8
  and ax, 0xf
  call printhexchar
  mov ax, si
  shr ax, 4
  and ax, 0xf
  call printhexchar
  mov ax, si
  and ax, 0xf
  call printhexchar
  pop si
  ret

printhexchar:
  cmp al, 10
  jge .alpha
.numeric:
  add al, '0'
  jmp .print
.alpha:
  add al, 'A' - 10
.print:
  call printchar
  ret

printcharntimes:
  mov si, ax
.start_print:
  cmp di, 0
  je .finish_print

  mov ax, si
  call printchar

  sub di, 1
  jmp .start_print
.finish_print:
  ret

; Arguments: character in al, cursor in bx
printchar:
  mov ah, 0x0e
  int 0x10
  ret

hellomessage:
  db "Hello, World!", 0

; Boot signature
times 510-($-$$) db 0
db 0x55
db 0xAA
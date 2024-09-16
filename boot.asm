ORG 0
BITS 16

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

.start_interrupt_setup:
  mov word[ss:0x0000], handle_div_zero
  mov word[ss:0x0002], 0x07c0

  int 0

.finish_interrupt_setup:

  mov cx, ss
  mov dx, sp
  mov si, hellomessage
  call printstr
  mov ax, cx
  call printhex
  mov ax, dx
  call printhex
  mov ax, cs
  call printhex
  lea ax, $
  call printhex

  int 0

  jmp $

handle_div_zero:
  pusha
  mov si, .div_zero_message
  call printstr
  popa
  iret

.div_zero_message:
  db "DIVZERO", 0

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
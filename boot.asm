ORG 0x7C00
BITS 16

start:
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
  jmp $

; TODO: Where is the stack pointing to when initialized?
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
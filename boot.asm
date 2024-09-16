ORG 0x7C00
BITS 16

start:
  mov si, hellomessage
  call printstr
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
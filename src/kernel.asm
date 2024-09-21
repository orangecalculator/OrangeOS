BITS 32

EXTERN kernel_main

GLOBAL _start
GLOBAL kernel_start

_start:
kernel_start:

.setup_stack:

%define STACK_BASE_ADDR 0x00200000 ; 2M

  mov ebp, STACK_BASE_ADDR
  mov esp, ebp

.enable_a20_line:
  ; Enable the A20 line
  in al, 0x92
  or al, 0x02
  out 0x92, al

  call kernel_main

  jmp $

; Align to sector granularity to avoid alignment issues
times 511 - ($-$$+511) % 512 db 0
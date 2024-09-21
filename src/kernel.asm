BITS 32

GLOBAL _start
GLOBAL kernel_start

db "HELLO FROM KERNEL SECTOR", 0x00

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

  jmp $

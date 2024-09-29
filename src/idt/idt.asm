
%macro define_interrupt_entrypoint 2
GLOBAL %1
EXTERN %2

%1:
  cli
  pushad
  call %2
  popad
  sti
  iret
%endmacro

define_interrupt_entrypoint irq_handler_entrypoint_timer, irq_handler_timer
define_interrupt_entrypoint irq_handler_entrypoint_keyboard, irq_handler_keyboard
define_interrupt_entrypoint irq_handler_entrypoint_primary_unknown, irq_handler_primary_unknown
define_interrupt_entrypoint irq_handler_entrypoint_secondary_unknown, irq_handler_secondary_unknown
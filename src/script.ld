ENTRY(_start)
OUTPUT_FORMAT(binary)

DISK_SECTOR_SIZE = 0x200;
DISK_SECTOR_SIZE_LOG2 = LOG2CEIL(DISK_SECTOR_SIZE);
ASSERT(DISK_SECTOR_SIZE == 1 << DISK_SECTOR_SIZE_LOG2, "Disk sector size is assumed to be power of 2.");

BOOT_RUNTIME_ADDR = 0x7c00;
BOOT_RUNTIME_SIZE = DISK_SECTOR_SIZE;
KERNEL_RUNTIME_ADDR = 1M;

DISK_BASE_ADDR = 0x00000000;
DISK_BOOT_ADDR = DISK_BASE_ADDR;
DISK_KERNEL_ADDR = DISK_BOOT_ADDR + BOOT_RUNTIME_SIZE;

ASSERT(DISK_KERNEL_ADDR % DISK_SECTOR_SIZE == 0, "Kernel disk address must align by sector granularity.")
DISK_KERNEL_ADDR_SECTOR = DISK_KERNEL_ADDR / DISK_SECTOR_SIZE;

MEMORY
{
  rom (RI) : ORIGIN = DISK_BASE_ADDR, LENGTH = 1024M
  boot (RX) : ORIGIN = BOOT_RUNTIME_ADDR, LENGTH = DISK_SECTOR_SIZE
  kernel (RX) : ORIGIN = KERNEL_RUNTIME_ADDR, LENGTH = 256M
}

SECTIONS
{

  .boot :
    ALIGN(DISK_SECTOR_SIZE)
  {
    *(.boot)
  } >boot AT>rom
  ASSERT(. - BOOT_RUNTIME_ADDR == BOOT_RUNTIME_SIZE || DEFINED(NO_BOOT_SECTOR),
    "Boot sector size should be exactly one sector.")

  .text :
    ALIGN(CONSTANT(COMMONPAGESIZE))
  {
    *(.text)
    *(.text.*)
    . = ALIGN(CONSTANT(COMMONPAGESIZE));
  } >kernel AT>rom

  .rodata :
    ALIGN(CONSTANT(COMMONPAGESIZE))
  {
    *(.rodata)
    . = ALIGN(CONSTANT(COMMONPAGESIZE));
  } >kernel AT>rom

  .data :
    ALIGN(CONSTANT(COMMONPAGESIZE))
  {
    *(.data)
    . = ALIGN(CONSTANT(COMMONPAGESIZE));
  } >kernel AT>rom

  .bss :
    ALIGN(CONSTANT(COMMONPAGESIZE))
  {
    *(COMMON)
    *(.bss)
    . = ALIGN(CONSTANT(COMMONPAGESIZE));
  } >kernel AT>rom

  /* Note that ATA reading succeeds even if binary file is not aligned up. */

  /* This is not to include .eh_frame, but to check to make sure it does not exist. */
  .eh_frame :
  {
    *(.eh_frame)
  }

  ASSERT(SIZEOF(.eh_frame) == 0, ".eh_frame should generally not be used in operating system code.")
}

TEXT_RUNTIME_ADDR = ADDR(.text);
TEXT_LOAD_ADDR = LOADADDR(.text);
TEXT_LOAD_ADDR_SECTOR = LOADADDR(.text) / DISK_SECTOR_SIZE;
TEXT_SECTION_SIZE = SIZEOF(.text);
TEXT_SECTION_SIZE_SECTOR = SIZEOF(.text) / DISK_SECTOR_SIZE;
ASSERT(TEXT_LOAD_ADDR % DISK_SECTOR_SIZE == 0, ".text section load address should align to disk sector size.");
ASSERT(TEXT_SECTION_SIZE % DISK_SECTOR_SIZE == 0, ".text section load size should align to disk sector size.");

RODATA_RUNTIME_ADDR = ADDR(.rodata);
RODATA_LOAD_ADDR = LOADADDR(.rodata);
RODATA_LOAD_ADDR_SECTOR = LOADADDR(.rodata) / DISK_SECTOR_SIZE;
RODATA_SECTION_SIZE = SIZEOF(.rodata);
RODATA_SECTION_SIZE_SECTOR = SIZEOF(.rodata) / DISK_SECTOR_SIZE;
ASSERT(RODATA_LOAD_ADDR % DISK_SECTOR_SIZE == 0, ".rodata section load address should align to disk sector size.");
ASSERT(RODATA_SECTION_SIZE % DISK_SECTOR_SIZE == 0, ".rodata section load size should align to disk sector size.");

DATA_RUNTIME_ADDR = ADDR(.data);
DATA_LOAD_ADDR = LOADADDR(.data);
DATA_LOAD_ADDR_SECTOR = LOADADDR(.data) / DISK_SECTOR_SIZE;
DATA_SECTION_SIZE = SIZEOF(.data);
DATA_SECTION_SIZE_SECTOR = SIZEOF(.data) / DISK_SECTOR_SIZE;
ASSERT(DATA_LOAD_ADDR % DISK_SECTOR_SIZE == 0, ".data section load address should align to disk sector size.");
ASSERT(DATA_SECTION_SIZE % DISK_SECTOR_SIZE == 0, ".data section load size should align to disk sector size.");

BSS_RUNTIME_ADDR = ADDR(.bss);
BSS_SECTION_SIZE = SIZEOF(.bss);
BSS_SECTION_SIZE_SECTOR = SIZEOF(.bss) / DISK_SECTOR_SIZE;
ASSERT(BSS_SECTION_SIZE % DISK_SECTOR_SIZE == 0, ".bss section load size should align to disk sector size.");
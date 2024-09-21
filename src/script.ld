ENTRY(_start)
OUTPUT_FORMAT(binary)

DISK_SECTOR_SIZE = 0x200;

BOOT_RUNTIME_START = 0x7c00;
BOOT_RUNTIME_SIZE = DISK_SECTOR_SIZE;
KERNEL_RUNTIME_START = 1M;
/* KERNEL_RUNTIME_SIZE should be determined below. */

DISK_BASE_ADDR = 0x00000000;
DISK_BOOT_ADDR = DISK_BASE_ADDR;
DISK_KERNEL_ADDR = DISK_BOOT_ADDR + BOOT_RUNTIME_SIZE;

ASSERT(DISK_KERNEL_ADDR % DISK_SECTOR_SIZE == 0, "Kernel disk address must align by sector granularity.")
DISK_KERNEL_ADDR_SECTOR = DISK_KERNEL_ADDR / DISK_SECTOR_SIZE;

SECTIONS
{

  . = BOOT_RUNTIME_START ;
  OVERLAY : AT(DISK_BASE_ADDR)
  {
    .boot
    {
      *(.boot)
    }
  }
  ASSERT(. - BOOT_RUNTIME_START == BOOT_RUNTIME_SIZE, "Boot sector size should be exactly one sector.")

  . = KERNEL_RUNTIME_START ;
  OVERLAY : AT(DISK_KERNEL_ADDR)
  {
    .text
    {
      *(.text)
    }

    .rodata
    {
      *(.rodata)
    }

    .data
    {
      *(.data)
    }

    .bss
    {
      *(COMMON)
      *(.bss)
    }

    /* Note that ATA reading succeeds even if binary file is not aligned up. */
  }
  KERNEL_RUNTIME_SIZE = . - KERNEL_RUNTIME_START ;
}

KERNEL_RUNTIME_SIZE_SECTOR = (KERNEL_RUNTIME_SIZE + DISK_SECTOR_SIZE - 1) / DISK_SECTOR_SIZE;
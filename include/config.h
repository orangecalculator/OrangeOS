#ifndef CONFIG_H
#define CONFIG_H

/**
 * This is a symbol in <src/boot/boot.asm>.
 * It should actually be asserted for equality,
 * but this seems to require gnu assembler,
 * so just rely on manual assertion for now.
 * In the future, this should be asserted for correctness
 * when assember is switched to gnu assembler.
 */
#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10

#define CONFIG_NUM_INTERRUPTS 0x100

#endif /* CONFIG_H */
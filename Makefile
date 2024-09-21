
DD := dd
MKDIR_P := mkdir -p
RM := rm -rf
TRUNCATE := truncate

QEMU := qemu-system-i386
GDB := gdb-multiarch

CROSS_COMPILE ?= x86_64-linux-gnu-

AR := $(CROSS_COMPILE)ar
AS := $(CROSS_COMPILE)as
CC := $(CROSS_COMPILE)gcc -m32
CXX := $(CROSS_COMPILE)g++ -m32
LD := $(CROSS_COMPILE)ld
NM := $(CROSS_COMPILE)nm
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
READELF := $(CROSS_COMPILE)readelf
STRIP := $(CROSS_COMPILE)strip

NASM := nasm

NASMFLAGS += -g
LDFLAGS += -g -O0

SRC_BOOT := src/boot/boot.asm
FOOTER_BOOT := src/boot/message.txt
OBJ_BOOT := build/$(SRC_BOOT).o
OUT_BOOT := bin/boot.bin

SRC_NASM := src/kernel.asm

OBJ := $(patsubst %,build/%.o,$(SRC_NASM))
OUT := bin/os.bin

LINKER_SCRIPT := src/script.ld
LDFLAGS += -ffreestanding -nostdlib

.PHONY: clean all dump_boot dump_boot16
clean:
	$(RM) bin/ build/

all: $(OUT)

run_boot: $(OUT_BOOT)
	$(QEMU) -hda $<

run_gdb_boot: $(OUT_BOOT)
	$(GDB) \
		-ex "target remote | $(QEMU) -hda $< -S -gdb stdio"

dump_boot: $(OUT_BOOT)
	$(OBJDUMP) -b binary -m i386 -D $<

dump_boot16: $(OUT_BOOT)
	$(OBJDUMP) -b binary -m i386 -D $< -Maddr16,data16

run: $(OUT)
	$(QEMU) -hda $<

run_gdb: $(OUT)
	$(GDB) \
		-ex "target remote | $(QEMU) -hda $< -S -gdb stdio"

dump: $(OUT)
	$(OBJDUMP) -b binary -m i386 -D $<

dump16: $(OUT)
	$(OBJDUMP) -b binary -m i386 -D $< -Maddr16,data16

$(OUT): $(OBJ_BOOT) $(OBJ)
	$(MKDIR_P) $(dir $@)
	# $(DD) if=$(OUT_BOOT) of=$(OUT)
	$(CC) -T $(LINKER_SCRIPT) -o $@ $^ $(LDFLAGS)

# Nasm build rule
$(OUT_BOOT): $(OBJ_BOOT) $(FOOTER_BOOT)
	$(MKDIR_P) $(dir $@)
	$(OBJCOPY) -O binary $(OBJ_BOOT) $(OUT_BOOT)
	$(DD) if=$(FOOTER_BOOT) of=$@ bs=512 count=1 seek=1
	$(TRUNCATE) --size=$$((2 * 512)) $@

build/%.asm.o: %.asm
	$(MKDIR_P) $(dir $@)
	$(NASM) $(NASMFLAGS) -f elf -o $@ $<
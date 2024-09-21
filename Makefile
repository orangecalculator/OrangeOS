
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
LD := $(CROSS_COMPILE)ld -melf_i386
NM := $(CROSS_COMPILE)nm
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
READELF := $(CROSS_COMPILE)readelf
STRIP := $(CROSS_COMPILE)strip

NASM := nasm

NASMFLAGS += -g
LDFLAGS += -g -O0
COMMONFLAGS += -g -O0

LDFLAGS += -relocatable

# Disable standard library facility
COMMONFLAGS += -ffreestanding -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -fno-pie -no-pie

# Prefer safety when compiling
COMMONFLAGS += -falign-jumps -falign-functions -falign-labels -falign-loops \
					-fstrength-reduce -fomit-frame-pointer -finline-functions \
					-Wl,--orphan-handling=discard
					# -fno-asynchronous-unwind-tables

# Leverage compiler diagnostics
COMMONFLAGS += -Wall -Werror -Wno-unused-functions -Wno-unused-label -Wno-cpp -Wno-unused-parameter

# Include directories in C
COMMONFLAGS += -Iinclude

CFLAGS += $(COMMONFLAGS)
CXXFLAGS += $(COMMONFLAGS)

CXXFLAGS += -fno-exceptions -fno-rtti

CFLAGS += -std=gnu11
CXXFLAGS += -std=gnu++17

SRC_BOOT := src/boot/boot.asm
FOOTER_BOOT := src/boot/message.txt
OBJ_BOOT := build/$(SRC_BOOT).o
OUT_BOOT := bin/boot.bin

SRC_NASM := src/kernel.asm
SRC_C := src/kernel.c
SRC_CXX := 

OBJ_KERNEL := $(patsubst %,build/%.o,$(SRC_NASM) $(SRC_C) $(SRC_CXX))
OUT_KERNEL := build/kernelfull.o

OUT := bin/os.bin

LINKER_SCRIPT := src/script.ld

.PHONY: clean all dump_boot dump_boot16 run run_gdb dump dump16
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

run_gdb: $(OUT) $(OUT_KERNEL)
	$(GDB) \
		-ex "target remote | $(QEMU) -hda $(OUT) -S -gdb stdio" \
		-ex "set architecture i386" \
		-ex "set confirm off" \
		-ex "add-symbol-file $(OUT_KERNEL) 0x100000" \
		-ex "set confirm on"

dump: $(OUT)
	$(OBJDUMP) -b binary -m i386 -D $<

dump16: $(OUT)
	$(OBJDUMP) -b binary -m i386 -D $< -Maddr16,data16

$(OUT): $(OBJ_BOOT) $(OUT_KERNEL) $(LINKER_SCRIPT)
	$(MKDIR_P) $(dir $@)
	$(CC) -T $(LINKER_SCRIPT) -o $@ $(OBJ_BOOT) $(OUT_KERNEL) $(CFLAGS)

# Nasm build rule
$(OUT_BOOT): $(OBJ_BOOT) $(FOOTER_BOOT)
	$(MKDIR_P) $(dir $@)
	$(OBJCOPY) -O binary $(OBJ_BOOT) $(OUT_BOOT)
	$(DD) if=$(FOOTER_BOOT) of=$@ bs=512 count=1 seek=1
	$(TRUNCATE) --size=$$((2 * 512)) $@

$(OUT_KERNEL): $(OBJ_KERNEL)
	$(LD) -o $@ $^ $(LDFLAGS)

build/%.asm.o: %.asm
	$(MKDIR_P) $(dir $@)
	$(NASM) $(NASMFLAGS) -f elf -o $@ $<

build/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

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
					-fno-asynchronous-unwind-tables
					# -Wl,--orphan-handling=error \

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
SRC_C := \
	src/kernel.c \
	src/display/terminal.c \
	src/memory/memory.c
SRC_CXX := 

SRC_NASM += src/idt/idt.asm
SRC_C += src/idt/idt.c

OBJ_KERNEL := $(patsubst %,build/%.o,$(SRC_NASM) $(SRC_C) $(SRC_CXX))
OUT_KERNEL := build/kernelfull.o

OUT_ELF := bin/os.elf
OUT := bin/os.bin

LINKER_SCRIPT := src/script.ld

.PHONY: clean all dump_boot dump_boot16 run run_gdb dump dump16
clean:
	$(RM) bin/ build/

all: $(OUT) $(OUT_ELF)

dump_boot: $(OUT_BOOT)
	$(OBJDUMP) -b binary -m i386 -D $<

dump_boot16: $(OUT_BOOT)
	$(OBJDUMP) -b binary -m i386 -D $< -Maddr16,data16

run: $(OUT)
	$(QEMU) -hda $<

run_gdb: $(OUT) $(OUT_ELF)
	$(GDB) $(OUT_ELF) \
		-ex "target remote | $(QEMU) -hda $(OUT) -S -gdb stdio" \
		-ex "set architecture i386"

run_gdb_server: $(OUT) $(OUT_ELF)
	$(QEMU) -hda $(OUT) -s -S

dump: $(OUT_ELF)
	$(OBJDUMP) -xdsrt $<

dump16: $(OUT_ELF)
	$(OBJDUMP) -xdsrt $< -Maddr16,data16

dump_kernel: $(OUT_KERNEL)
	$(OBJDUMP) -m i386 -xdsrt $<

$(OUT): $(OBJ_BOOT) $(OUT_KERNEL) $(LINKER_SCRIPT)
	$(MKDIR_P) $(dir $@)
	$(CC) -T $(LINKER_SCRIPT) -o $@ $(OBJ_BOOT) $(OUT_KERNEL) $(CFLAGS)

$(OUT_ELF): $(OBJ_BOOT) $(OUT_KERNEL) $(LINKER_SCRIPT)
	$(MKDIR_P) $(dir $@)
	$(CC) -Wl,--oformat=elf32-i386 -T $(LINKER_SCRIPT) -o $@ $(OBJ_BOOT) $(OUT_KERNEL) $(CFLAGS)

# Nasm build rule
$(OUT_BOOT): $(OBJ_BOOT) $(FOOTER_BOOT)
	$(MKDIR_P) $(dir $@)
	$(OBJCOPY) -O binary $(OBJ_BOOT) $(OUT_BOOT)
	$(DD) if=$(FOOTER_BOOT) of=$@ bs=512 count=1 seek=1
	$(TRUNCATE) --size=$$((2 * 512)) $@

$(OUT_KERNEL): $(OBJ_KERNEL)
	$(MKDIR_P) $(dir $@)
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
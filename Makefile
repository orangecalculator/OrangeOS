
DD := dd
MKDIR_P := mkdir -p
RM := rm -rf
TRUNCATE := truncate

CROSS_COMPILE ?= x86_64-linux-gnu-

AR := $(CROSS_COMPILE)ar
AS := $(CROSS_COMPILE)as
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
NM := $(CROSS_COMPILE)nm
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
READELF := $(CROSS_COMPILE)readelf
STRIP := $(CROSS_COMPILE)strip

NASM := nasm

SRC_BOOT := src/boot/boot.asm
FOOTER_BOOT := src/boot/message.txt
OUT_BOOT := bin/boot.bin

OBJ := $(patsubst %,build/%.o,$(SRC_NASM))

.PHONY: clean all dump_boot dump_boot16
clean:
	$(RM) bin/ build/

all: $(OUT_BOOT)

dump_boot: $(OUT_BOOT)
	$(OBJDUMP) -b binary -m i386 -D $<

dump_boot16: $(OUT_BOOT)
	$(OBJDUMP) -b binary -m i386 -D $< -Maddr16,data16

# Nasm build rule
$(OUT_BOOT): $(SRC_BOOT) $(FOOTER_BOOT)
	$(MKDIR_P) $(dir $@)
	$(NASM) -f bin -o $@ $<
	$(DD) if=$(FOOTER_BOOT) of=$@ bs=512 count=1 seek=1
	$(TRUNCATE) --size=$$((2 * 512)) $@
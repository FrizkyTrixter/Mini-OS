# Makefile for myos2 project (with GRUB ISO + QEMU)

# ─── Tools ────────────────────────────────────────────────────────────────────
CC              := gcc
ASM             := nasm
LD              := ld
OBJCOPY         := objcopy
GRUB_MKRESCUE   := grub-mkrescue
QEMU            := qemu-system-i386

# ─── Directories ──────────────────────────────────────────────────────────────
SRC_DIR         := src
INC_DIR         := include
BUILD_DIR       := build
ISO_DIR         := iso

# ─── Flags ─────────────────────────────────────────────────────────────────────
CFLAGS          := -std=gnu99 -m32 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector \
                    -I$(INC_DIR) -I$(SRC_DIR)
ASFLAGS         := -f elf32
LDFLAGS         := -m elf_i386 -Ttext 0x1000

# ─── Sources ───────────────────────────────────────────────────────────────────
C_SRCS := \
  $(SRC_DIR)/console.c \
  $(SRC_DIR)/gdt.c \
  $(SRC_DIR)/idt.c \
  $(SRC_DIR)/kernel.c \
  $(SRC_DIR)/paging.c \
  $(SRC_DIR)/ports.c \
  $(SRC_DIR)/pic.c

# assemble both NASM‐style and GNU‐asm style
ASM_SRCS_S      := $(wildcard $(SRC_DIR)/*.s)
ASM_SRCS_ASM    := $(wildcard $(SRC_DIR)/*.asm)

# ─── Objects ───────────────────────────────────────────────────────────────────
OBJS := \
  $(patsubst $(SRC_DIR)/%.c,   $(BUILD_DIR)/%.o, $(C_SRCS))    \
  $(patsubst $(SRC_DIR)/%.s,   $(BUILD_DIR)/%.o, $(ASM_SRCS_S)) \
  $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SRCS_ASM))

# ─── Outputs ───────────────────────────────────────────────────────────────────
KERNEL_ELF      := $(BUILD_DIR)/kernel.elf
KERNEL_BIN      := $(BUILD_DIR)/kernel.bin
ISO_IMAGE       := $(BUILD_DIR)/myos.iso

# ─── Top‐level targets ────────────────────────────────────────────────────────
.PHONY: all clean run

all: $(KERNEL_BIN) $(ISO_IMAGE)

run: all
	@echo "🖥  Launching QEMU with $(ISO_IMAGE)…"
	$(QEMU) -cdrom $(ISO_IMAGE) -serial stdio

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR)

# ─── Build steps ─────────────────────────────────────────────────────────────
# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble `.s` files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(BUILD_DIR)
	$(ASM) $(ASFLAGS) $< -o $@

# Assemble `.asm` files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(BUILD_DIR)
	$(ASM) $(ASFLAGS) $< -o $@

# Link into an ELF
$(KERNEL_ELF): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# Make a raw binary out of the ELF
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# ─── ISO (GRUB) ───────────────────────────────────────────────────────────────
$(ISO_IMAGE): $(KERNEL_ELF)
	@echo "🗄  Building ISO tree in '$(ISO_DIR)/'…"
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/grub

	# Copy the ELF (must have a Multiboot header!)
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf

	# GRUB config: immediately boot our kernel
	printf "set timeout=0\n\
menuentry \"MyTinyOS\" {\n\
    multiboot /boot/kernel.elf\n\
    boot\n\
}\n" > $(ISO_DIR)/boot/grub/grub.cfg

	@echo "📀 Generating ISO $(ISO_IMAGE)…"
	$(GRUB_MKRESCUE) -o $(ISO_IMAGE) $(ISO_DIR)

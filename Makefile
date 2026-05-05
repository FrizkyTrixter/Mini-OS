# ============================================================================
#   Mini-OS Build System (GRUB ISO + QEMU)
# ============================================================================

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

# ─── Flags ────────────────────────────────────────────────────────────────────
CFLAGS          := -std=gnu99 -m32 -ffreestanding -O2 -Wall -Wextra \
                  -fno-stack-protector -fno-pic -fno-pie \
                  -I$(INC_DIR) -I$(SRC_DIR)

ASFLAGS         := -f elf32
LDFLAGS         := -m elf_i386 -T linker.ld

# ─── C Sources ────────────────────────────────────────────────────────────────
C_SRCS := \
  $(SRC_DIR)/console.c \
  $(SRC_DIR)/gdt.c \
  $(SRC_DIR)/idt.c \
  $(SRC_DIR)/kernel.c \
  $(SRC_DIR)/paging.c \
  $(SRC_DIR)/pmm.c \
  $(SRC_DIR)/ports.c \
  $(SRC_DIR)/pic.c \
  $(SRC_DIR)/heap.c \
  $(SRC_DIR)/timer.c \
  $(SRC_DIR)/exceptions.c \
  $(SRC_DIR)/scheduler.c

# ─── Assembly Sources ─────────────────────────────────────────────────────────
ASM_SRCS_S      := $(wildcard $(SRC_DIR)/*.s)
ASM_SRCS_ASM    := $(wildcard $(SRC_DIR)/*.asm)

# ─── Objects ──────────────────────────────────────────────────────────────────
OBJS := \
  $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
  $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.o,$(ASM_SRCS_S)) \
  $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS_ASM))

# ─── Outputs ──────────────────────────────────────────────────────────────────
KERNEL_ELF      := $(BUILD_DIR)/kernel.elf
KERNEL_BIN      := $(BUILD_DIR)/kernel.bin
ISO_IMAGE       := $(BUILD_DIR)/myos.iso

# ─── Top-level Targets ────────────────────────────────────────────────────────
.PHONY: all clean run qemu iso

all: $(KERNEL_BIN) $(ISO_IMAGE)

run: all
	@echo "🖥️  Launching QEMU with $(ISO_IMAGE)…"
	$(QEMU) -cdrom $(ISO_IMAGE) -serial stdio -no-reboot -no-shutdown

qemu: run

clean:
	@echo "🧹 Cleaning build directories…"
	rm -rf $(BUILD_DIR) $(ISO_DIR)

# ─── Build Rules ──────────────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(BUILD_DIR)
	$(ASM) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(BUILD_DIR)
	$(ASM) $(ASFLAGS) $< -o $@

$(KERNEL_ELF): $(OBJS) linker.ld
	@mkdir -p $(BUILD_DIR)
	@echo "🔗 Linking $(KERNEL_ELF)…"
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "📦 Creating flat binary $(KERNEL_BIN)…"
	$(OBJCOPY) -O binary $< $@

# ─── ISO Build ────────────────────────────────────────────────────────────────
$(ISO_IMAGE): $(KERNEL_ELF)
	@echo "🗄️  Building ISO tree in '$(ISO_DIR)/'…"
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf

	printf "set timeout=0\n\
menuentry \"MyTinyOS\" {\n\
    multiboot /boot/kernel.elf\n\
    boot\n\
}\n" > $(ISO_DIR)/boot/grub/grub.cfg

	@echo "📀 Generating ISO $(ISO_IMAGE)…"
	$(GRUB_MKRESCUE) -o $(ISO_IMAGE) $(ISO_DIR)
	@echo "✅ ISO ready: $(ISO_IMAGE)"
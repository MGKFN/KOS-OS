CC      := x86_64-elf-gcc
CXX     := x86_64-elf-g++
LD      := x86_64-elf-ld
ASM     := nasm

ASMFLAGS := -f elf64

CFLAGS := \
    -ffreestanding       \
    -fno-builtin         \
    -fno-stack-protector \
    -nostdlib            \
    -mno-red-zone        \
    -mno-sse             \
    -mno-sse2            \
    -mno-mmx             \
    -mno-80387           \
    -m64                 \
    -O2                  \
    -Wall                \
    -Wextra              \
    -Ikernel

CXXFLAGS := \
    $(CFLAGS)            \
    -fno-exceptions      \
    -fno-rtti            \
    -std=c++17

LDFLAGS := \
    -T kernel/linker.ld  \
    -nostdlib

BUILD_DIR := build

KERNEL_OBJS := \
    $(BUILD_DIR)/boot.o            \
    $(BUILD_DIR)/interrupts_asm.o  \
    $(BUILD_DIR)/interrupts_cpp.o  \
    $(BUILD_DIR)/idt.o             \
    $(BUILD_DIR)/pic.o             \
    $(BUILD_DIR)/keyboard.o        \
    $(BUILD_DIR)/pmm.o             \
    $(BUILD_DIR)/heap.o            \
    $(BUILD_DIR)/shell.o           \
    $(BUILD_DIR)/rtc.o             \
    $(BUILD_DIR)/kernel.o

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
ISO_FILE    := kdos.iso

.PHONY: all clean run

all: $(ISO_FILE)
	@echo "KOS Checkpoint 12 complete"

$(BUILD_DIR)/boot.o: kernel/boot.asm | $(BUILD_DIR)
	@echo "[ASM] $<"
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/interrupts_asm.o: kernel/interrupts.asm | $(BUILD_DIR)
	@echo "[ASM] $<"
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/interrupts_cpp.o: kernel/interrupts.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: kernel/idt.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/pic.o: kernel/pic.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: kernel/keyboard.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/pmm.o: kernel/pmm.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/heap.o: kernel/heap.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: kernel/shell.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/rtc.o: kernel/rtc.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.cpp | $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KERNEL_ELF): $(KERNEL_OBJS)
	@echo "[LD] Linking"
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

$(ISO_FILE): $(KERNEL_ELF)
	@mkdir -p iso_root/boot/grub
	@cp $(KERNEL_ELF) iso_root/boot/kernel.elf
	@cp boot/grub/grub.cfg iso_root/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO_FILE) iso_root 2>/dev/null
	@echo "[ISO] Created"

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR) iso_root $(ISO_FILE)

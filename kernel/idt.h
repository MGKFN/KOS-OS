// ============================================================
//  K-DOS-1.0-Beta  —  idt.h
//  Interrupt Descriptor Table definitions.
//
//  The IDT tells the CPU what function to call when an
//  interrupt or exception occurs. Think of it as a jump
//  table for hardware events and CPU exceptions.
//
//  In 64-bit long mode each IDT entry is 16 bytes.
//  We need 256 entries (one per interrupt vector 0-255).
// ============================================================
#pragma once

#include <stdint.h>

// ============================================================
//  IDT Entry (Gate Descriptor) — 16 bytes in 64-bit mode
//
//  The CPU uses this structure to find the handler function
//  when interrupt vector N fires.
//
//  Structure layout:
//    [15:0]   offset_low    — bits 0-15 of handler address
//    [31:16]  selector      — GDT code segment selector (0x08)
//    [34:32]  ist           — Interrupt Stack Table index (0=disabled)
//    [39:35]  reserved      — must be zero
//    [43:40]  gate_type     — 0xE = 64-bit interrupt gate
//    [44]     zero          — must be zero
//    [46:45]  dpl           — Descriptor Privilege Level (0=kernel)
//    [47]     present       — 1 = this entry is valid
//    [63:48]  offset_mid    — bits 16-31 of handler address
//    [95:64]  offset_high   — bits 32-63 of handler address
//    [127:96] reserved2     — must be zero
// ============================================================
struct IDTEntry {
    uint16_t offset_low;    // Handler address bits 0-15
    uint16_t selector;      // GDT code segment (always 0x08 for kernel)
    uint8_t  ist;           // Interrupt Stack Table (0 = not used)
    uint8_t  type_attr;     // Type and attributes byte
    uint16_t offset_mid;    // Handler address bits 16-31
    uint32_t offset_high;   // Handler address bits 32-63
    uint32_t reserved;      // Must be zero
} __attribute__((packed));  // No padding — CPU reads this raw

// ============================================================
//  IDT Pointer — passed to the LIDT instruction
//  Same concept as the GDT pointer we already have.
// ============================================================
struct IDTPointer {
    uint16_t limit;         // Size of IDT in bytes minus 1
    uint64_t base;          // Linear address of the IDT array
} __attribute__((packed));

// ============================================================
//  Gate type constants for type_attr byte
//
//  Format: Present(1) | DPL(2) | Zero(1) | Type(4)
//
//  Interrupt Gate (0x8E): Present=1, DPL=0, Type=0xE
//    → CPU automatically disables interrupts on entry (IF flag cleared)
//    → Best for hardware IRQs and most exceptions
//
//  Trap Gate (0x8F): Present=1, DPL=0, Type=0xF
//    → CPU does NOT disable interrupts on entry
//    → Used for software exceptions where we want interrupts on
// ============================================================
static const uint8_t IDT_GATE_INTERRUPT = 0x8E;  // Kernel interrupt gate
static const uint8_t IDT_GATE_TRAP      = 0x8F;  // Kernel trap gate

// ============================================================
//  Public API
// ============================================================

// Initialize the IDT — sets up all 256 entries and loads LIDT
void idt_init();

// Set a single IDT entry
// vector   : interrupt number (0-255)
// handler  : address of the ASM stub function
// type_attr: gate type (use constants above)
void idt_set_entry(uint8_t vector, uint64_t handler, uint8_t type_attr);

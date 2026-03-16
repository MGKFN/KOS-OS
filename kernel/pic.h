// ============================================================
//  K-DOS-1.0-Beta  —  pic.h
//  8259 Programmable Interrupt Controller (PIC) driver.
//
//  The PIC sits between hardware devices and the CPU.
//  It manages 16 hardware interrupt lines (IRQ0-IRQ15)
//  across two cascaded chips (master + slave).
//
//  Problem: By default the PIC maps IRQs 0-7 to CPU vectors
//  8-15, which CONFLICTS with CPU exception vectors 8-15.
//  We must REMAP the PIC so IRQs land at vectors 32-47
//  (safely above all 32 CPU exceptions).
// ============================================================
#pragma once
#include <stdint.h>

// PIC I/O port addresses
// Each PIC has a command port and a data port
static const uint16_t PIC1_COMMAND = 0x20;  // Master PIC command
static const uint16_t PIC1_DATA    = 0x21;  // Master PIC data
static const uint16_t PIC2_COMMAND = 0xA0;  // Slave PIC command
static const uint16_t PIC2_DATA    = 0xA1;  // Slave PIC data

// End-of-interrupt command — sent to PIC after handling an IRQ
// Without this, the PIC won't send any more interrupts
static const uint8_t PIC_EOI = 0x20;

// Vector offsets after remapping
static const uint8_t PIC1_OFFSET = 32;  // IRQ0-7  → vectors 32-39
static const uint8_t PIC2_OFFSET = 40;  // IRQ8-15 → vectors 40-47

// Initialize and remap both PICs
void pic_init();

// Send End-of-Interrupt signal to PIC(s)
// Must be called at the END of every hardware IRQ handler
// irq: the IRQ number (0-15), NOT the vector number
void pic_send_eoi(uint8_t irq);

// Mask (disable) a specific IRQ line
void pic_mask_irq(uint8_t irq);

// Unmask (enable) a specific IRQ line
void pic_unmask_irq(uint8_t irq);

// Helper: write a byte to an I/O port
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Helper: read a byte from an I/O port
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Helper: small I/O delay (write to unused port 0x80)
// Some old hardware needs a tiny pause between I/O operations
static inline void io_wait() {
    outb(0x80, 0);
}

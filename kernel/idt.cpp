// ============================================================
//  K-DOS-1.0-Beta  —  idt.cpp
//  Interrupt Descriptor Table implementation.
//
//  This file:
//    1. Declares the 256-entry IDT array
//    2. Provides idt_set_entry() to configure individual gates
//    3. Provides idt_init() to wire up all CPU exceptions,
//       hardware IRQs, and load the IDT via LIDT
// ============================================================

#include "idt.h"
#include "pic.h"
#include <stdint.h>

// ============================================================
//  The actual IDT — 256 entries × 16 bytes = 4096 bytes
//  Aligned to 16 bytes for best CPU performance.
// ============================================================
static IDTEntry idt[256] __attribute__((aligned(16)));

// IDT pointer struct passed to LIDT
static IDTPointer idt_ptr;

// ============================================================
//  External ASM interrupt stubs declared in interrupts.asm
//  Each one saves CPU state, calls the C++ handler, restores.
//
//  CPU Exceptions (vectors 0-31, defined by Intel spec):
// ============================================================
extern "C" {
    // CPU Exceptions
    void isr0();   // Division by zero
    void isr1();   // Debug
    void isr2();   // Non-maskable interrupt
    void isr3();   // Breakpoint
    void isr4();   // Overflow
    void isr5();   // Bound range exceeded
    void isr6();   // Invalid opcode
    void isr7();   // Device not available
    void isr8();   // Double fault
    void isr9();   // Coprocessor segment overrun
    void isr10();  // Invalid TSS
    void isr11();  // Segment not present
    void isr12();  // Stack fault
    void isr13();  // General protection fault
    void isr14();  // Page fault
    void isr15();  // Reserved
    void isr16();  // x87 FPU error
    void isr17();  // Alignment check
    void isr18();  // Machine check
    void isr19();  // SIMD FP exception
    void isr20();  // Virtualization exception
    void isr21();  // Reserved
    void isr22();  // Reserved
    void isr23();  // Reserved
    void isr24();  // Reserved
    void isr25();  // Reserved
    void isr26();  // Reserved
    void isr27();  // Reserved
    void isr28();  // Reserved
    void isr29();  // Reserved
    void isr30();  // Security exception
    void isr31();  // Reserved

    // Hardware IRQs (vectors 32-47 after PIC remapping)
    // IRQ0  = System timer (PIT)
    // IRQ1  = PS/2 Keyboard
    // IRQ2  = Cascade (links master/slave PIC)
    // IRQ3  = COM2 serial
    // IRQ4  = COM1 serial
    // IRQ5  = LPT2 parallel
    // IRQ6  = Floppy disk
    // IRQ7  = LPT1 / spurious
    // IRQ8  = CMOS real-time clock
    // IRQ9  = Free / ACPI
    // IRQ10 = Free
    // IRQ11 = Free
    // IRQ12 = PS/2 Mouse
    // IRQ13 = FPU
    // IRQ14 = ATA Primary
    // IRQ15 = ATA Secondary
    void irq0();
    void irq1();
    void irq2();
    void irq3();
    void irq4();
    void irq5();
    void irq6();
    void irq7();
    void irq8();
    void irq9();
    void irq10();
    void irq11();
    void irq12();
    void irq13();
    void irq14();
    void irq15();
}

// ============================================================
//  idt_set_entry()
//  Configure one slot in the IDT.
//
//  We split the 64-bit handler address across three fields
//  as required by the CPU's IDT entry format.
// ============================================================
void idt_set_entry(uint8_t vector, uint64_t handler, uint8_t type_attr)
{
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].selector    = 0x08;     // Kernel code segment from GDT
    idt[vector].ist         = 0;        // No interrupt stack table
    idt[vector].type_attr   = type_attr;
    idt[vector].reserved    = 0;
}

// ============================================================
//  idt_init()
//  Wire up all interrupt vectors and load the IDT.
//
//  Called once from kernel_main() during boot.
// ============================================================
void idt_init()
{
    // --- Set IDT pointer ---
    idt_ptr.limit = (sizeof(IDTEntry) * 256) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    // --- Zero all entries first ---
    for (int i = 0; i < 256; i++) {
        idt_set_entry(i, 0, 0);
    }

    // --- CPU Exception handlers (vectors 0-31) ---
    // These fire when the CPU encounters an error condition.
    // If we don't handle them, we get a triple fault (reboot).
    idt_set_entry(0,  (uint64_t)isr0,  IDT_GATE_INTERRUPT);
    idt_set_entry(1,  (uint64_t)isr1,  IDT_GATE_INTERRUPT);
    idt_set_entry(2,  (uint64_t)isr2,  IDT_GATE_INTERRUPT);
    idt_set_entry(3,  (uint64_t)isr3,  IDT_GATE_TRAP);
    idt_set_entry(4,  (uint64_t)isr4,  IDT_GATE_TRAP);
    idt_set_entry(5,  (uint64_t)isr5,  IDT_GATE_INTERRUPT);
    idt_set_entry(6,  (uint64_t)isr6,  IDT_GATE_INTERRUPT);
    idt_set_entry(7,  (uint64_t)isr7,  IDT_GATE_INTERRUPT);
    idt_set_entry(8,  (uint64_t)isr8,  IDT_GATE_INTERRUPT);
    idt_set_entry(9,  (uint64_t)isr9,  IDT_GATE_INTERRUPT);
    idt_set_entry(10, (uint64_t)isr10, IDT_GATE_INTERRUPT);
    idt_set_entry(11, (uint64_t)isr11, IDT_GATE_INTERRUPT);
    idt_set_entry(12, (uint64_t)isr12, IDT_GATE_INTERRUPT);
    idt_set_entry(13, (uint64_t)isr13, IDT_GATE_INTERRUPT);
    idt_set_entry(14, (uint64_t)isr14, IDT_GATE_INTERRUPT);
    idt_set_entry(15, (uint64_t)isr15, IDT_GATE_INTERRUPT);
    idt_set_entry(16, (uint64_t)isr16, IDT_GATE_INTERRUPT);
    idt_set_entry(17, (uint64_t)isr17, IDT_GATE_INTERRUPT);
    idt_set_entry(18, (uint64_t)isr18, IDT_GATE_INTERRUPT);
    idt_set_entry(19, (uint64_t)isr19, IDT_GATE_INTERRUPT);
    idt_set_entry(20, (uint64_t)isr20, IDT_GATE_INTERRUPT);
    idt_set_entry(21, (uint64_t)isr21, IDT_GATE_INTERRUPT);
    idt_set_entry(22, (uint64_t)isr22, IDT_GATE_INTERRUPT);
    idt_set_entry(23, (uint64_t)isr23, IDT_GATE_INTERRUPT);
    idt_set_entry(24, (uint64_t)isr24, IDT_GATE_INTERRUPT);
    idt_set_entry(25, (uint64_t)isr25, IDT_GATE_INTERRUPT);
    idt_set_entry(26, (uint64_t)isr26, IDT_GATE_INTERRUPT);
    idt_set_entry(27, (uint64_t)isr27, IDT_GATE_INTERRUPT);
    idt_set_entry(28, (uint64_t)isr28, IDT_GATE_INTERRUPT);
    idt_set_entry(29, (uint64_t)isr29, IDT_GATE_INTERRUPT);
    idt_set_entry(30, (uint64_t)isr30, IDT_GATE_INTERRUPT);
    idt_set_entry(31, (uint64_t)isr31, IDT_GATE_INTERRUPT);

    // --- Hardware IRQ handlers (vectors 32-47) ---
    // After PIC remapping, IRQ0 = vector 32, IRQ1 = vector 33, etc.
    idt_set_entry(32, (uint64_t)irq0,  IDT_GATE_INTERRUPT);
    idt_set_entry(33, (uint64_t)irq1,  IDT_GATE_INTERRUPT);
    idt_set_entry(34, (uint64_t)irq2,  IDT_GATE_INTERRUPT);
    idt_set_entry(35, (uint64_t)irq3,  IDT_GATE_INTERRUPT);
    idt_set_entry(36, (uint64_t)irq4,  IDT_GATE_INTERRUPT);
    idt_set_entry(37, (uint64_t)irq5,  IDT_GATE_INTERRUPT);
    idt_set_entry(38, (uint64_t)irq6,  IDT_GATE_INTERRUPT);
    idt_set_entry(39, (uint64_t)irq7,  IDT_GATE_INTERRUPT);
    idt_set_entry(40, (uint64_t)irq8,  IDT_GATE_INTERRUPT);
    idt_set_entry(41, (uint64_t)irq9,  IDT_GATE_INTERRUPT);
    idt_set_entry(42, (uint64_t)irq10, IDT_GATE_INTERRUPT);
    idt_set_entry(43, (uint64_t)irq11, IDT_GATE_INTERRUPT);
    idt_set_entry(44, (uint64_t)irq12, IDT_GATE_INTERRUPT);
    idt_set_entry(45, (uint64_t)irq13, IDT_GATE_INTERRUPT);
    idt_set_entry(46, (uint64_t)irq14, IDT_GATE_INTERRUPT);
    idt_set_entry(47, (uint64_t)irq15, IDT_GATE_INTERRUPT);

    // --- Load the IDT ---
    // LIDT reads the pointer structure and activates our IDT.
    // After this, any interrupt will use our handlers.
    asm volatile ("lidt %0" : : "m"(idt_ptr));
}

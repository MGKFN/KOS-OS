#include "interrupts.h"

extern void vga_print(const char* str, unsigned char color);
extern void vga_print_hex(uint64_t val, unsigned char color);

extern "C" void keyboard_handler();

extern "C" void isr_handler(uint64_t vector, uint64_t error_code, uint64_t rip) {
    vga_print("\n\n  *** KERNEL EXCEPTION ***\n", 0x0C);
    vga_print("  Exception: ", 0x0C);
    
    const char* exception_names[] = {
        "Division By Zero", "Debug", "NMI", "Breakpoint", "Overflow",
        "Bound Range", "Invalid Opcode", "Device Not Available", "Double Fault",
        "Coprocessor Segment", "Invalid TSS", "Segment Not Present",
        "Stack Fault", "General Protection", "Page Fault", "Reserved",
        "x87 FPU Error", "Alignment Check", "Machine Check", "SIMD FP Exception"
    };
    
    if (vector < 20) {
        vga_print(exception_names[vector], 0x0C);
    } else {
        vga_print("Unknown", 0x0C);
    }
    
    vga_print("\n  Vector:     0x", 0x0F);
    vga_print_hex(vector, 0x0F);
    vga_print("\n  Error Code: 0x", 0x0F);
    vga_print_hex(error_code, 0x0F);
    vga_print("\n  RIP:        0x", 0x0F);
    vga_print_hex(rip, 0x0F);
    vga_print("\n\n  System halted.\n", 0x0C);
    
    while(1) {
        asm volatile("cli; hlt");
    }
}

extern "C" void irq_handler(uint64_t irq_num) {
    if (irq_num == 1) {
        keyboard_handler();
    }
    
    // Send EOI to PIC
    if (irq_num >= 8) {
        asm volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
    }
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}

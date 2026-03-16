#include "pic.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

void pic_init()
{
    // ICW1: Begin initialization
    outb(PIC1_COMMAND, 0x11);   io_wait();
    outb(PIC2_COMMAND, 0x11);   io_wait();

    // ICW2: Remap vectors
    outb(PIC1_DATA, 32);        io_wait();  // IRQ 0-7  -> INT 32-39
    outb(PIC2_DATA, 40);        io_wait();  // IRQ 8-15 -> INT 40-47

    // ICW3: Setup cascade
    outb(PIC1_DATA, 0x04);      io_wait();  // Master: slave on IRQ2
    outb(PIC2_DATA, 0x02);      io_wait();  // Slave: cascade identity

    // ICW4: 8086 mode
    outb(PIC1_DATA, 0x01);      io_wait();
    outb(PIC2_DATA, 0x01);      io_wait();

    // Unmask all IRQs
    outb(PIC1_DATA, 0x00);      io_wait();
    outb(PIC2_DATA, 0x00);      io_wait();
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
}

// ============================================================
//  KOS Interrupt Handlers
// ============================================================
#pragma once
#include <stdint.h>

void interrupts_init();

extern "C" void isr_handler(uint64_t vector, uint64_t error_code, uint64_t rip);
extern "C" void irq_handler(uint64_t irq_num);

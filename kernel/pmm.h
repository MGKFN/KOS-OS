// ============================================================
//  K-DOS CP7 — Physical Memory Manager
//  Bitmap allocator voor 4KB page frames
// ============================================================
#pragma once
#include <stdint.h>

// Initialize PMM with Multiboot2 memory map
void pmm_init(uint64_t multiboot_info_addr);

// Allocate one 4KB physical page frame
// Returns physical address or 0 if out of memory
uint64_t pmm_alloc_page();

// Free a physical page frame
void pmm_free_page(uint64_t phys_addr);

// Get total/free memory stats in KB
uint64_t pmm_get_total_memory();
uint64_t pmm_get_free_memory();

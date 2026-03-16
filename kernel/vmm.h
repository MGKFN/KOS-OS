// ============================================================
//  KOS CP8 — Virtual Memory Manager
//  4-level paging: PML4 → PDPT → PD → PT
// ============================================================
#pragma once
#include <stdint.h>

// Page flags
#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITABLE   (1 << 1)
#define PAGE_USER       (1 << 2)

// Initialize VMM — set up kernel page tables
void vmm_init();

// Map a virtual address to a physical address
bool vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags);

// Unmap a virtual address
void vmm_unmap_page(uint64_t virt);

// Get physical address for a virtual address (returns 0 if not mapped)
uint64_t vmm_get_physical(uint64_t virt);

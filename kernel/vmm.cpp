// ============================================================
//  KOS CP8 — Virtual Memory Manager (Huge Pages Edition)
// ============================================================
#include "vmm.h"
#include "pmm.h"
#include <stdint.h>

extern void vga_print(const char* str, unsigned char color);
extern void vga_print_hex(uint64_t val, unsigned char color);

#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE (2 * 1024 * 1024)  // 2MB
#define PAGE_HUGE (1 << 7)

typedef uint64_t* PageTable;

static PageTable kernel_pml4 = nullptr;

static inline uint64_t pte_get_address(uint64_t entry) {
    return entry & 0x000FFFFFFFFFF000ULL;
}

static inline bool pte_is_present(uint64_t entry) {
    return (entry & 1) != 0;
}

static inline uint64_t pte_create(uint64_t phys_addr, uint32_t flags) {
    return (phys_addr & 0x000FFFFFFFFFF000ULL) | (flags & 0xFFF);
}

static inline uint64_t pml4_index(uint64_t virt) { return (virt >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t virt) { return (virt >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t virt)   { return (virt >> 21) & 0x1FF; }

static PageTable alloc_page_table() {
    uint64_t phys = pmm_alloc_page();
    if (phys == 0) return nullptr;
    
    volatile uint64_t* table = (volatile uint64_t*)phys;
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
    return (PageTable)phys;
}

void vmm_init() {
    vga_print("  [VMM] Using 2MB huge pages\n", 0x0E);
    
    kernel_pml4 = alloc_page_table();
    if (!kernel_pml4) {
        vga_print("  [VMM] ERROR: No PML4\n", 0x0C);
        return;
    }
    
    vga_print("  [VMM] PML4: 0x", 0x0A);
    vga_print_hex((uint64_t)kernel_pml4, 0x0A);
    vga_print("\n", 0x0A);
    
    // Map first 16MB using 2MB pages (only 8 iterations!)
    vga_print("  [VMM] Mapping 0-16MB...\n", 0x0E);
    
    // Allocate PDPT
    uint64_t pml4_i = pml4_index(0);
    PageTable pdpt = alloc_page_table();
    if (!pdpt) {
        vga_print("  [VMM] No PDPT\n", 0x0C);
        return;
    }
    kernel_pml4[pml4_i] = pte_create((uint64_t)pdpt, PAGE_PRESENT | PAGE_WRITABLE);
    
    // Allocate PD
    uint64_t pdpt_i = pdpt_index(0);
    PageTable pd = alloc_page_table();
    if (!pd) {
        vga_print("  [VMM] No PD\n", 0x0C);
        return;
    }
    pdpt[pdpt_i] = pte_create((uint64_t)pd, PAGE_PRESENT | PAGE_WRITABLE);
    
    // Map 8 huge pages (8 x 2MB = 16MB)
    for (int i = 0; i < 8; i++) {
        uint64_t phys = i * HUGE_PAGE_SIZE;
        pd[i] = pte_create(phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE);
    }
    
    vga_print("  [VMM] Loading CR3...\n", 0x0E);
    asm volatile("mov %0, %%cr3" :: "r"((uint64_t)kernel_pml4) : "memory");
    
    vga_print("  [VMM] Success!\n", 0x0A);
}

bool vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags) {
    uint64_t pml4_i = pml4_index(virt);
    uint64_t pdpt_i = pdpt_index(virt);
    uint64_t pd_i   = pd_index(virt);
    uint64_t pt_i   = (virt >> 12) & 0x1FF;
    
    PageTable pdpt;
    if (!pte_is_present(kernel_pml4[pml4_i])) {
        pdpt = alloc_page_table();
        if (!pdpt) return false;
        kernel_pml4[pml4_i] = pte_create((uint64_t)pdpt, PAGE_PRESENT | PAGE_WRITABLE);
    } else {
        pdpt = (PageTable)pte_get_address(kernel_pml4[pml4_i]);
    }
    
    PageTable pd;
    if (!pte_is_present(pdpt[pdpt_i])) {
        pd = alloc_page_table();
        if (!pd) return false;
        pdpt[pdpt_i] = pte_create((uint64_t)pd, PAGE_PRESENT | PAGE_WRITABLE);
    } else {
        pd = (PageTable)pte_get_address(pdpt[pdpt_i]);
    }
    
    PageTable pt;
    if (!pte_is_present(pd[pd_i])) {
        pt = alloc_page_table();
        if (!pt) return false;
        pd[pd_i] = pte_create((uint64_t)pt, PAGE_PRESENT | PAGE_WRITABLE);
    } else {
        pt = (PageTable)pte_get_address(pd[pd_i]);
    }
    
    pt[pt_i] = pte_create(phys, flags);
    
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    
    return true;
}

void vmm_unmap_page(uint64_t virt) {
    // Not implemented yet
    (void)virt;
}

uint64_t vmm_get_physical(uint64_t virt) {
    // Not implemented yet
    (void)virt;
    return 0;
}

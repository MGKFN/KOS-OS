// ============================================================
//  KOS CP9 — Kernel Heap Implementation
// ============================================================
#include "heap.h"
#include "pmm.h"

extern void vga_print(const char* str, unsigned char color);
extern void vga_print_hex(uint64_t val, unsigned char color);

#define PAGE_SIZE 4096

// Heap block header
struct HeapBlock {
    uint64_t size;        // Size of usable memory (not including header)
    bool free;            // Is this block free?
    HeapBlock* next;      // Next block in list
    uint64_t magic;       // Magic number for validation (0xDEADBEEF)
} __attribute__((packed));

static const uint64_t HEAP_MAGIC = 0xDEADBEEF;
static const uint64_t MIN_BLOCK_SIZE = 32;

static HeapBlock* heap_start = nullptr;
static uint64_t total_allocated = 0;
static uint64_t total_freed = 0;

void heap_init() {
    vga_print("  [HEAP] Initializing kernel heap...\n", 0x0E);
    
    // Allocate first page for heap
    uint64_t phys = pmm_alloc_page();
    if (phys == 0) {
        vga_print("  [HEAP] ERROR: Cannot allocate initial page\n", 0x0C);
        return;
    }
    
    heap_start = (HeapBlock*)phys;
    heap_start->size = PAGE_SIZE - sizeof(HeapBlock);
    heap_start->free = true;
    heap_start->next = nullptr;
    heap_start->magic = HEAP_MAGIC;
    
    vga_print("  [HEAP] Start: 0x", 0x0A);
    vga_print_hex((uint64_t)heap_start, 0x0A);
    vga_print("\n", 0x0A);
    
    vga_print("  [HEAP] Initial size: ", 0x0A);
    vga_print_hex(heap_start->size, 0x0A);
    vga_print(" bytes\n", 0x0A);
}

// Align size to 8-byte boundary
static uint64_t align_size(uint64_t size) {
    return (size + 7) & ~7ULL;
}

// Expand heap by allocating more pages
static bool expand_heap(uint64_t needed_size) {
    uint64_t pages_needed = (needed_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // Find last block
    HeapBlock* last = heap_start;
    while (last->next) last = last->next;
    
    // Allocate pages
    for (uint64_t i = 0; i < pages_needed; i++) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) return false;
        
        HeapBlock* new_block = (HeapBlock*)phys;
        new_block->size = PAGE_SIZE - sizeof(HeapBlock);
        new_block->free = true;
        new_block->next = nullptr;
        new_block->magic = HEAP_MAGIC;
        
        last->next = new_block;
        last = new_block;
    }
    
    return true;
}

void* kmalloc(uint64_t size) {
    if (size == 0) return nullptr;
    
    size = align_size(size);
    
    // Find free block
    HeapBlock* current = heap_start;
    while (current) {
        if (current->magic != HEAP_MAGIC) {
            vga_print("  [HEAP] CORRUPTION DETECTED!\n", 0x0C);
            return nullptr;
        }
        
        if (current->free && current->size >= size) {
            // Split block if remainder is large enough
            if (current->size >= size + sizeof(HeapBlock) + MIN_BLOCK_SIZE) {
                HeapBlock* new_block = (HeapBlock*)((uint8_t*)current + sizeof(HeapBlock) + size);
                new_block->size = current->size - size - sizeof(HeapBlock);
                new_block->free = true;
                new_block->next = current->next;
                new_block->magic = HEAP_MAGIC;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->free = false;
            total_allocated += current->size;
            
            return (void*)((uint8_t*)current + sizeof(HeapBlock));
        }
        
        current = current->next;
    }
    
    // No suitable block — expand heap
    if (!expand_heap(size + sizeof(HeapBlock))) {
        return nullptr;
    }
    
    // Try again
    return kmalloc(size);
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    HeapBlock* block = (HeapBlock*)((uint8_t*)ptr - sizeof(HeapBlock));
    
    if (block->magic != HEAP_MAGIC) {
        vga_print("  [HEAP] Invalid free()!\n", 0x0C);
        return;
    }
    
    block->free = true;
    total_freed += block->size;
    
    // Coalesce with next block if free
    if (block->next && block->next->free) {
        block->size += sizeof(HeapBlock) + block->next->size;
        block->next = block->next->next;
    }
    
    // TODO: Coalesce with previous block (needs prev pointer)
}

uint64_t heap_get_used() {
    return total_allocated - total_freed;
}

uint64_t heap_get_free() {
    uint64_t free_bytes = 0;
    HeapBlock* current = heap_start;
    while (current) {
        if (current->free) free_bytes += current->size;
        current = current->next;
    }
    return free_bytes;
}

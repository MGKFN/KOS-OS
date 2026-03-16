// ============================================================
//  K-DOS CP7 — Physical Memory Manager Implementation
// ============================================================
#include "pmm.h"
#include <stdint.h>

extern void vga_print(const char* str, unsigned char color);
extern void vga_print_hex(uint64_t val, unsigned char color);

#define PAGE_SIZE 4096

// Multiboot2 tag types
#define MULTIBOOT_TAG_TYPE_END      0
#define MULTIBOOT_TAG_TYPE_MMAP     6

// Multiboot2 structures
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot_mmap_entry entries[1];
} __attribute__((packed));

// Bitmap allocator
static uint8_t* bitmap = nullptr;
static uint64_t bitmap_size = 0;
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;
static uint64_t max_memory = 0;

// Simple pointer — we use static buffer before heap exists
static uint8_t static_bitmap[32768]; // 32KB = supports up to 1GB RAM

static void bitmap_set(uint64_t page_num) {
    uint64_t byte = page_num / 8;
    uint64_t bit  = page_num % 8;
    if (byte < bitmap_size) {
        bitmap[byte] |= (1 << bit);
    }
}

static void bitmap_clear(uint64_t page_num) {
    uint64_t byte = page_num / 8;
    uint64_t bit  = page_num % 8;
    if (byte < bitmap_size) {
        bitmap[byte] &= ~(1 << bit);
    }
}

static bool bitmap_test(uint64_t page_num) {
    uint64_t byte = page_num / 8;
    uint64_t bit  = page_num % 8;
    if (byte >= bitmap_size) return true;
    return (bitmap[byte] & (1 << bit)) != 0;
}

void pmm_init(uint64_t multiboot_info_addr) {
    vga_print("  [PMM] Multiboot2 addr: 0x", 0x0E);
    vga_print_hex(multiboot_info_addr, 0x0E);
    vga_print("\n", 0x0E);

    multiboot_tag* tag = (multiboot_tag*)((uintptr_t)multiboot_info_addr + 8);
    
    vga_print("  [PMM] Scanning memory map...\n", 0x0E);
    
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            multiboot_tag_mmap* mmap = (multiboot_tag_mmap*)tag;
            uint32_t entry_count = (mmap->size - 16) / mmap->entry_size;
            
            for (uint32_t i = 0; i < entry_count; i++) {
                multiboot_mmap_entry* e = &mmap->entries[i];
                if (e->type == 1) {
                    uint64_t end = e->addr + e->len;
                    if (end > max_memory) max_memory = end;
                }
            }
        }
        
        tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7));
    }

    total_pages = max_memory / PAGE_SIZE;
    bitmap_size = (total_pages + 7) / 8;
    
    if (bitmap_size > sizeof(static_bitmap)) {
        bitmap_size = sizeof(static_bitmap);
        total_pages = bitmap_size * 8;
    }
    
    bitmap = static_bitmap;
    
    for (uint64_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;
    }
    
    tag = (multiboot_tag*)((uintptr_t)multiboot_info_addr + 8);
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            multiboot_tag_mmap* mmap = (multiboot_tag_mmap*)tag;
            uint32_t entry_count = (mmap->size - 16) / mmap->entry_size;
            
            for (uint32_t i = 0; i < entry_count; i++) {
                multiboot_mmap_entry* e = &mmap->entries[i];
                if (e->type == 1) {
                    uint64_t start_page = e->addr / PAGE_SIZE;
                    uint64_t page_count = e->len / PAGE_SIZE;
                    
                    for (uint64_t p = start_page; p < start_page + page_count && p < total_pages; p++) {
                        bitmap_clear(p);
                        free_pages++;
                    }
                }
            }
        }
        tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7));
    }
    
    for (uint64_t p = 0; p < 512; p++) {
        if (!bitmap_test(p)) free_pages--;
        bitmap_set(p);
    }
    
    vga_print("  [PMM] Total RAM: ", 0x0A);
    vga_print_hex(max_memory / 1024, 0x0A);
    vga_print(" KB\n", 0x0A);
    
    vga_print("  [PMM] Free pages: ", 0x0A);
    vga_print_hex(free_pages, 0x0A);
    vga_print(" (", 0x0A);
    vga_print_hex((free_pages * PAGE_SIZE) / 1024, 0x0A);
    vga_print(" KB)\n", 0x0A);
}

uint64_t pmm_alloc_page() {
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            return i * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page < total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

uint64_t pmm_get_total_memory() {
    return total_pages * PAGE_SIZE;
}

uint64_t pmm_get_free_memory() {
    return free_pages * PAGE_SIZE;
}

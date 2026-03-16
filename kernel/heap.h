// ============================================================
//  KOS CP9 — Kernel Heap Allocator
//  Simple linked-list allocator
// ============================================================
#pragma once
#include <stdint.h>

// Initialize heap — call once at boot
void heap_init();

// Allocate memory
void* kmalloc(uint64_t size);

// Free memory
void kfree(void* ptr);

// Get heap statistics
uint64_t heap_get_used();
uint64_t heap_get_free();

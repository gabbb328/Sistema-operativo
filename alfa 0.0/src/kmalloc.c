#include "kmalloc.h"

static uint8_t heap_memory[8 * 1024 * 1024]; 
static uint32_t heap_current = 0;
static uint32_t heap_size = 8 * 1024 * 1024;

void init_kmalloc() {
    heap_current = 0;
}

void* kmalloc(size_t size) {
    if (size % 4 != 0) {
        size += 4 - (size % 4);
    }
    
    if (heap_current + size >= heap_size) {
        return 0;
    }
    
    uint32_t ptr = (uint32_t)&heap_memory[heap_current];
    heap_current += size;
    return (void*)ptr;
}

void kfree(void* ptr) {
    (void)ptr; 
}

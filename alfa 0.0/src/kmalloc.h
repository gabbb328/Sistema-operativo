#ifndef KMALLOC_H
#define KMALLOC_H
#include <stddef.h>
#include <stdint.h>

void init_kmalloc(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif

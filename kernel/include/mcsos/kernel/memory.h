#ifndef MCSOS_MEMORY_H
#define MCSOS_MEMORY_H

#include <stddef.h>

void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);

void kernel_memory_init(void);

#endif

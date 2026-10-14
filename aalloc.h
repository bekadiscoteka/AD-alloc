#ifndef AALLOC_H
#define AALLOC_H

#include <stddef.h>

void *aalloc(size_t size);
void afree(void *ptr);
void print_heaps(void);

#endif

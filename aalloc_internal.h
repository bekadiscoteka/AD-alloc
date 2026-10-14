#ifndef AALLOC_INTERNAL_H
#define AALLOC_INTERNAL_H

#include "aalloc.h"

#define HEAPSIZE 4064
#define ALIGN_UP_SIZE(n) \
	(((n) + _Alignof(max_align_t) - 1) / _Alignof(max_align_t) * _Alignof(max_align_t))
#define HEAP_HEADER_SIZE ALIGN_UP_SIZE(sizeof(struct s_heap))
#define BLOCK_HEADER_SIZE ALIGN_UP_SIZE(sizeof(struct s_block))
#define HEAP_SHIFT(h) ((struct s_block *)((char *)(h) + HEAP_HEADER_SIZE))
#define BLOCK_SHIFT(b) ((void *)((char *)(b) + BLOCK_HEADER_SIZE))

struct s_heap {
	struct s_heap *next;
	struct s_heap *prev;
	size_t fsize;
	size_t blocks;
	size_t fblocks;
};

struct s_block {
	struct s_block *next;
	struct s_block *prev;
	size_t size;
	short freed;
};

#ifdef AALLOC_TESTING
extern struct s_heap *HEAD;
extern size_t heapcount;
struct s_heap *getheap(struct s_block *block);
#endif

#endif

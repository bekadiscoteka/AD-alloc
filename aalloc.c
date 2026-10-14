#include "aalloc_internal.h"

#define RESERVE 1

#include <assert.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#ifdef DEBUG

	#define LOG(fmt, ...)\
		fprintf(stderr, "[DEBUG] %s:%d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else 

	#define LOG(fmt, ...) do {} while (0)

#endif

#ifdef AALLOC_TESTING
struct s_heap *HEAD = NULL;
size_t heapcount = 0;
#else
static struct s_heap *HEAD = NULL;
static size_t heapcount = 0;
#endif

void print_heaps(void) {
	printf("Heap count: %zu\n", heapcount);
	if (HEAD == NULL) {
		puts("No heaps allocated.");
		return;
	}

	size_t index = 0;
	struct s_heap *h = HEAD;
	do {
		printf("Heap %zu: free bytes=%zu, blocks=%zu, free blocks=%zu\n",
			index++, h->fsize, h->blocks, h->fblocks);
		h = h->next;
	} while (h != HEAD);
}

/* get heap header of current block */
#ifdef AALLOC_TESTING
struct s_heap *
#else
static struct s_heap *
#endif
getheap(struct s_block *b)
{
	while (b->prev != NULL) 
		b = b->prev;
	struct s_heap *h = (struct s_heap *)((char *)b - HEAP_HEADER_SIZE);

	return h;
}

/* derive one block with fixed memory from existing block's memory and cascade them.
	Return new block's addr, NULL in fail
*/

static struct s_block*
bfork(struct s_block *b, size_t nbyte, int flag) 
{

	size_t remainder = b->size - nbyte - BLOCK_HEADER_SIZE;
	if (remainder == 0)
		return NULL;

	struct s_block *newb = (struct s_block *)((char *)BLOCK_SHIFT(b) + remainder);
	newb->freed	= 1;
	newb->size	= nbyte;
	newb->next	= b->next;
	newb->prev	= b;

	b->next = newb;
	if (newb->next != NULL)
		newb->next->prev = newb;
	b->size = remainder;

	struct s_heap *h = getheap(b);
	h->blocks++;
	h->fblocks++;
	h->fsize -= BLOCK_HEADER_SIZE;
	if (RESERVE && flag) {
		h->fsize -= newb->size;
		h->fblocks--;	
		newb->freed = 0;
	}

	return newb;
}

/* allocate a new heap and insert it at HEAD */
static int heapalloc(void) {
	struct s_heap *newh;

	newh = (struct s_heap *)mmap(NULL, HEAPSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 1, 0);
	if (newh == (void*) -1)
		return -1;

	newh->fsize	= HEAPSIZE - HEAP_HEADER_SIZE;
	newh->blocks	= 1;
	newh->fblocks	= 1;
	if (HEAD == NULL) {
		newh->next = newh;
		newh->prev = newh;
	} else {
		newh->next = HEAD;
		newh->prev = HEAD->prev;
		HEAD->prev->next = newh;
		HEAD->prev = newh;
	}
	HEAD = newh;
	heapcount++;


	struct s_block *b = HEAP_SHIFT(newh);
	b->next = NULL;
	b->prev = NULL;
	b->freed = 1;
	b->size = (newh->fsize -= BLOCK_HEADER_SIZE);

	return 0;
}


/* 
	merges given block with following block
	works with only free blocks
*/
static struct s_block *
bmerge( struct s_block *b ) {
	assert( b->freed && b->next->freed );
	LOG("merging blocks:\n \t%zu\t%zu \n \t|\t/ \n \t%zu \n", b->size, b->next->size, b->size + b->next->size);

	struct s_block *rm = b->next;
	
	b->size += rm->size + BLOCK_HEADER_SIZE;
	b->next = rm->next;
	if (rm->next != NULL) 
		rm->next->prev = b;

	struct s_heap *h = getheap(b);
	h->blocks--;
	h->fblocks--;
	h->fsize += BLOCK_HEADER_SIZE;

	return b;
}


/* custom malloc() implementation */
void *aalloc(size_t nbyte) {
	const size_t alignment = _Alignof(max_align_t);
	const size_t max_size = HEAPSIZE - HEAP_HEADER_SIZE - BLOCK_HEADER_SIZE;
	if (nbyte > SIZE_MAX - (alignment - 1))
		return NULL;
	nbyte = ALIGN_UP_SIZE(nbyte);
	if (nbyte > max_size)
		return NULL;

	struct s_heap *start = HEAD;
	if (start != NULL) {
		struct s_heap *h = start;
		do {
			struct s_block *b = HEAP_SHIFT(h);
			while (b != NULL) {
				if (b->freed) {
					while (b->next != NULL && b->next->freed)
						bmerge(b);

					if (b->size > nbyte + BLOCK_HEADER_SIZE) {
						b = bfork(b, nbyte, RESERVE);
						HEAD = h->next;
						return BLOCK_SHIFT(b);
					}
					if (b->size >= nbyte) {
						b->freed = 0;
						h->fsize -= b->size;
						h->fblocks--;
						HEAD = h->next;
						return BLOCK_SHIFT(b);
					}
				}
				b = b->next;
			}
			h = h->next;
		} while (h != start);
	}

	if (heapalloc() == -1) 
		return NULL;

	return aalloc(nbyte);
}

/*	free heap completely
	RETURN VALUE: 0 = success, -1 = fail
*/
static int heapfree( struct s_heap *rmh ) {
	LOG("tried to HEAPFREE\n");
	if ( rmh == NULL )
		return -1;

	struct s_heap *next = rmh->next;
	struct s_heap *prev = rmh->prev;
	int is_single_heap = next == rmh;
	if ( munmap(rmh, HEAPSIZE) == -1 ) {
		warn("munmap");
		return -1;
	}
	heapcount--;

	if (is_single_heap) {
		HEAD = NULL;
		return 0;
	}

	prev->next = next;
	next->prev = prev;
	if (rmh == HEAD)
		HEAD = next;

	return 0;
}

/* custom free() */
void afree(void *p) {
	if (p == NULL)
		return;

	struct s_block *b = (struct s_block *)((char *)p - BLOCK_HEADER_SIZE);
	if (b->freed)
		return;
	b->freed = 1;

	size_t size = b->size;

	struct s_heap *h = getheap(b);
	h->fsize += size;
	h->fblocks++;

	LOG("afree(): freeblocks: %zu\tblocks: %zu\n", h->fblocks, h->blocks);

	if (h->fblocks == h->blocks)
		heapfree(h);
}

#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <err.h>

#define HEAPSIZE 4064
#define ALIGN_UP_SIZE(n) \
	(((n) + _Alignof(max_align_t) - 1) / _Alignof(max_align_t) * _Alignof(max_align_t))
#define HEAP_HEADER_SIZE ALIGN_UP_SIZE(sizeof(struct s_heap))
#define BLOCK_HEADER_SIZE ALIGN_UP_SIZE(sizeof(struct s_block))
#define HEAP_SHIFT(h) ((struct s_block *)((char *)(h) + HEAP_HEADER_SIZE))
#define BLOCK_SHIFT(b) ((void *)((char *)(b) + BLOCK_HEADER_SIZE))

#define RESERVE 1

#ifdef DEBUG

	#define LOG(fmt, ...)\
		fprintf(stderr, "[DEBUG] %s:%d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else 

	#define LOG(fmt, ...) do {} while (0)

#endif

struct s_heap {
	struct s_heap	*next,
			*prev;

	size_t		fsize;
	size_t		blocks;
	size_t		fblocks;
};

struct s_block {
	struct s_block	*next;
	struct s_block	*prev;
	
	size_t		size;
	
	short		freed;
};

struct s_heap *HEAD = NULL;

size_t heapcount = 0;

size_t getheapc(void) {
	return heapcount;
}

void heapinfo(struct s_heap *h) {
	printf( "blockcount: %zu\n"
			"free memory in heap: %zu\n", h->blocks, h->fsize);
};

void blockinfo(struct s_block *b) {
	printf(	"block size: %zu\n"
			"block status: %s\n"
			, b->size, b->freed == 0 ? "NOT FREE\n" : "FREE\n" );
}

/* get heap header of current block */
struct s_heap *
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

struct s_block*
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

/* forks block from front */
struct s_block*
bfork_front(struct s_block *b, size_t nbyte, int flag) 
{
	size_t remain = b->size - nbyte - BLOCK_HEADER_SIZE;
	assert(remain > 0);
	
	struct s_block *newb = (struct s_block *)((char *)BLOCK_SHIFT(b) + nbyte);
	newb->freed	= 1;
	newb->size	= remain;	
	newb->prev	= b;
	newb->next	= b->next;

	b->size = nbyte;
	b->next	= newb;
	if (newb->next != NULL)
		newb->next->prev = newb;

	struct s_heap *h = getheap(b);
	h->blocks++;
	h->fblocks++;
	h->fsize -= BLOCK_HEADER_SIZE;
	if (RESERVE && flag) {
		h->fsize -= b->size;
		h->fblocks--;
		b->freed = 0;
	}

	return b;
}

/* allocate a new heap and insert it at HEAD */
int heapalloc(void) {
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
struct s_block *
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
int heapfree( struct s_heap *rmh ) {
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

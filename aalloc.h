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

	size_t			fsize;
	size_t			blocks;
	size_t			fblocks;
};

struct s_block {
	struct s_block	*next;
	struct s_block	*prev;
	
	size_t			size;
	
	short			freed;
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
	newb->freed = 1;
	newb->size	= nbyte;
	newb->next	= b->next;
	newb->prev	= b;

	b->next = newb;
	b->size = remainder;

	struct s_heap *h = getheap(b);
	h->blocks++;
	h->fblocks++;
	h->fsize -= BLOCK_HEADER_SIZE;
	if (RESERVE && flag) {
		h->fsize -= newb->size;
		h->fblocks--;	
		b->freed = 0;
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

/* allocate a new heap, cascade it from HEAD. Return status: 0 - OK, 1-fail with errno */
int heapalloc(void) {
	struct s_heap *newh;

	newh = (struct s_heap *)mmap(NULL, HEAPSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 1, 0);
	if (newh == (void*) -1)  
		return -1;

	newh->fsize	= HEAPSIZE - HEAP_HEADER_SIZE;
	newh->next		= HEAD;
	newh->blocks	= 1;
	newh->fblocks	= 1;
	HEAD			= newh;
	heapcount++;


	struct s_block *b = HEAP_SHIFT(newh);
	b->next = NULL;
	b->prev = NULL;
	b->freed = 1;
	b->size = (newh->fsize -= BLOCK_HEADER_SIZE);

	return 0;
}

struct s_heap*
heapfree(struct s_heap *h, struct s_heap *rmh) {
	LOG("tried to HEAPFREE\n");
	if (h == NULL)
		return NULL;

	if (h == rmh) {
		struct s_heap *next = rmh->next;
		if (munmap(rmh, HEAPSIZE) == -1) {
			warn("munmap");
			return h;
		}
		heapcount--;
		return next;
	} 

	h->next = heapfree(h->next, rmh);
	return h;
}

/* 
merges given block and the next block
merge only free blocks
*/

struct s_block *
bmerge( struct s_block *b ) {
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

	struct s_heap *h;
	for ( h=HEAD; h != NULL; h=h->next ) {

		if (h->fsize >= nbyte) {

			struct s_block* b;
			enum {IDLE, STREAK} state = IDLE;
			for ( b=HEAP_SHIFT(h); b != NULL; b=b->next ) {
				if ( b->freed ) {

					if (b->size > nbyte + BLOCK_HEADER_SIZE) {
						b = bfork_front(b, nbyte, RESERVE);
						return BLOCK_SHIFT(b);
					}

					else if (b->size == nbyte) {
						b->freed = 0;
						h->fsize -= nbyte;
						h->fblocks--;
						return BLOCK_SHIFT(b);
					}

					else if (state == STREAK) 
						b = bmerge(b->prev);

					state = STREAK;

				}
				else state = IDLE;
			}

		}


	}
	
	if (heapalloc() == -1) 
		return NULL;

	return aalloc(nbyte);
}

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
		HEAD = heapfree(HEAD, h);
}

#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>
#include <stddef.h>
#include <string.h>

#define HEAPSIZE 4064
#define HEAP_SHIFT(h)	( (void*) h + sizeof(struct s_heap)	)	
#define BLOCK_SHIFT(b)	( (void*) b + sizeof(struct s_block) )

struct s_heap {
	struct s_heap	*next;

	size_t			free_size;
	size_t			blocks;
};

struct s_block {
	struct s_block	*next;
	struct s_block	*prev;
	
	size_t			size;
	
	short			freed;
};

#if (defined(DEBUG) || defined(TEST)) 
static 
#endif
struct s_heap *HEAD = NULL;

static size_t heapcount = 0;

size_t getheapc(void) {
	return heapcount;
}

void heapinfo(struct s_heap *h) {
	printf( "blockcount: %zu\n"
			"free memory in heap: %zu\n", h->blocks, h->free_size);
};

void blockinfo(struct s_block *b) {
	printf(	"block size: %zu\n"
			"block status: %s\n"
			, b->size, b->freed == 0 ? "NOT FREE\n" : "FREE\n" );
}

/* derive one block with fixed memory from existing block's memory and cascade them. Return new block's addr*/
void *block_fork(struct s_block *b, size_t nbyte) {

	size_t remainder = b->size - nbyte - sizeof(struct s_block);

	assert(remainder > 0);

	struct s_block *newb = (struct s_block*) ( BLOCK_SHIFT(b) + remainder ); 
	newb->freed = 1;
	newb->size	= nbyte;
	newb->next	= b->next;
	newb->prev	= b;

	b->next = newb;
	b->size = remainder;

	while (b->prev != NULL) 
		b = b->prev;
	struct s_heap *h = (struct s_heap*) ( ((void*)b) - sizeof(struct s_heap));
	h->blocks++;

	return newb;
}

/* allocate a new heap, cascade it from HEAD. Return status: 0 - OK, 1-fail with errno */
int heap_alloc(void) {
	struct s_heap *newh;

	newh = (struct s_heap *)mmap(NULL, HEAPSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 1, 0);
	if (newh == (void*) -1)  
		return -1;

	newh->free_size	= HEAPSIZE - sizeof(struct s_heap);
	newh->next		= HEAD;
	newh->blocks	= 1;
	HEAD			= newh;
	heapcount++;


	struct s_block *b = HEAP_SHIFT(newh);
	b->next = NULL;
	b->prev = NULL;
	b->freed = 1;
	b->size = (newh->free_size -= sizeof(struct s_block));

	return 0;
}

/* custom malloc() implementation */
void *aalloc(size_t nbyte) {
	assert(nbyte < HEAPSIZE);

	struct s_heap *h;
	for ( h=HEAD; h != NULL; h=h->next ) {

		if (h->free_size >= nbyte) {

			struct s_block* b;
			for ( b=HEAP_SHIFT(h); b != NULL; b=b->next ) {
				if ( b->freed && (b->size > nbyte + sizeof(struct s_block)) ) {
					b = block_fork(b, nbyte);
					h->free_size -= sizeof(struct s_block);
				}

				if ( b->freed && (b->size == nbyte) ) {
					b->freed = 0;
					h->free_size -= nbyte;
					return BLOCK_SHIFT(b);
				}
			}

		}


	}
	
	if (heap_alloc() == -1) 
		return NULL;

	return aalloc(nbyte);
}

void afree(void *p) {
	struct s_block *b = (struct s_block*) (p - sizeof(struct s_block));
	b->freed = 1;

	size_t size = b->size;

	while (b->prev != NULL) 
		b = b->prev;
	
	struct s_heap *h = (struct s_heap*) ( ((void*) b) - sizeof(struct s_heap) );
	h->free_size += size;
}







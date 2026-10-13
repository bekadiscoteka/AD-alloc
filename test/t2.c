#define DEBUG

#include <aalloc.h>
#include <assert.h>

int main(void) {
	void *first = aalloc(64);
	void *second = aalloc(128);
	struct s_heap *heap = HEAD;

	assert(first != NULL);
	assert(second != NULL);
	assert(heap != NULL);
	assert(heap->blocks == 3);
	assert(heap->fblocks == 1);

	afree(first);
	assert(HEAD == heap);
	assert(heap->fblocks == 2);

	afree(second);
	assert(HEAD == NULL);
	assert(heapcount == 0);

	void *older_heap_block = aalloc(3000);
	struct s_heap *older_heap = HEAD;
	void *newer_heap_block = aalloc(3000);
	struct s_heap *newer_heap = HEAD;

	assert(older_heap_block != NULL);
	assert(newer_heap_block != NULL);
	assert(newer_heap != older_heap);
	assert(newer_heap->next == older_heap);
	assert(heapcount == 2);

	afree(older_heap_block);
	assert(HEAD == newer_heap);
	assert(newer_heap->next == NULL);
	assert(heapcount == 1);

	afree(newer_heap_block);
	assert(HEAD == NULL);
	assert(heapcount == 0);

	afree(NULL);
	return 0;
}

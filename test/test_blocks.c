#include <aalloc_internal.h>

#include <stdio.h>

static int failures;

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
			__FILE__, __LINE__, #condition); \
		failures++; \
	} \
} while (0)

static int check_single_heap(const char *test_name) {
	if (HEAD == NULL || HEAD->next != HEAD || HEAD->prev != HEAD) {
		fprintf(stderr, "%s: expected one circularly linked heap\n", test_name);
		return 0;
	}

	size_t blocks = 0;
	size_t free_blocks = 0;
	size_t free_bytes = 0;
	struct s_block *previous = NULL;
	struct s_block *b = HEAP_SHIFT(HEAD);
	char *heap_end = (char *)HEAD + HEAPSIZE;

	while (b != NULL) {
		blocks++;
		if (b->prev != previous) {
			fprintf(stderr, "%s: broken block prev link\n", test_name);
			return 0;
		}
		if (b->freed) {
			free_blocks++;
			free_bytes += b->size;
		}

		char *next_address = (char *)b + BLOCK_HEADER_SIZE + b->size;
		if (b->next == NULL) {
			if (next_address != heap_end) {
				fprintf(stderr, "%s: blocks do not span heap\n", test_name);
				return 0;
			}
		} else if ((char *)b->next != next_address) {
			fprintf(stderr, "%s: blocks are not contiguous\n", test_name);
			return 0;
		}

		previous = b;
		b = b->next;
	}

	if (blocks != HEAD->blocks || free_blocks != HEAD->fblocks ||
		free_bytes != HEAD->fsize) {
		fprintf(stderr, "%s: heap counters disagree\n", test_name);
		return 0;
	}
	return 1;
}

static void test_reuse_and_coalescing(void) {
	void *first = aalloc(64);
	void *second = aalloc(128);
	void *guard = aalloc(64);
	void *filler = aalloc(3500);
	CHECK(first != NULL && second != NULL && guard != NULL && filler != NULL);
	if (first == NULL || second == NULL || guard == NULL || filler == NULL)
		return;

	afree(first);
	CHECK(check_single_heap("first free"));
	afree(second);
	CHECK(check_single_heap("adjacent frees"));

	void *merged_space = aalloc(160);
	CHECK(merged_space != NULL);
	if (merged_space != NULL) {
		struct s_block *allocated = (struct s_block *)
			((char *)merged_space - BLOCK_HEADER_SIZE);
		char *lower = (char *)first < (char *)second
			? (char *)first : (char *)second;
		char *upper = (char *)first > (char *)second
			? (char *)first : (char *)second;
		CHECK((char *)merged_space >= lower);
		CHECK((char *)merged_space < upper);
		CHECK(allocated->freed == 0);
		CHECK(allocated->size >= 160);
	}
	CHECK(check_single_heap("allocation after coalescing"));

	if (merged_space != NULL)
		afree(merged_space);
	afree(guard);
	afree(filler);
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
}

int main(void) {
	test_reuse_and_coalescing();

	if (failures != 0) {
		fprintf(stderr, "%d block checks failed\n", failures);
		return 1;
	}
	puts("Block reuse and coalescing tests passed.");
	return 0;
}

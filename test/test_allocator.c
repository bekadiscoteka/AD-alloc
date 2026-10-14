#include <aalloc.h>
#include <stdint.h>

static int failures;

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
			__FILE__, __LINE__, #condition); \
		failures++; \
	} \
} while (0)

static int check_invariants(const char *test_name) {
	size_t heaps = 0;

	for (struct s_heap *h = HEAD; h != NULL; h = h->next) {
		size_t blocks = 0;
		size_t free_blocks = 0;
		size_t free_bytes = 0;
		struct s_block *previous = NULL;
		struct s_block *b = (struct s_block *)((char *)h + HEAP_HEADER_SIZE);
		char *heap_end = (char *)h + HEAPSIZE;

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

		if (blocks != h->blocks || free_blocks != h->fblocks ||
			free_bytes != h->fsize) {
			fprintf(stderr,
				"%s: heap counters disagree "
				"(blocks %zu/%zu, free blocks %zu/%zu, free bytes %zu/%zu)\n",
				test_name, blocks, h->blocks, free_blocks, h->fblocks,
				free_bytes, h->fsize);
			return 0;
		}
		heaps++;
	}

	if (heaps != heapcount) {
		fprintf(stderr, "%s: heapcount is %zu, linked heaps are %zu\n",
			test_name, heapcount, heaps);
		return 0;
	}

	return 1;
}

static void test_empty_and_null_free(void) {
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);

	afree(NULL);

	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
	CHECK(check_invariants("empty/null free"));
}

static void test_alignment_and_writable_memory(void) {
	const size_t sizes[] = {1, 3, 7, 16, 31, 64, 257};
	void *allocations[sizeof(sizes) / sizeof(sizes[0])];

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		allocations[i] = aalloc(sizes[i]);
		CHECK(allocations[i] != NULL);
		if (allocations[i] == NULL)
			return;
		CHECK((uintptr_t)allocations[i] % _Alignof(max_align_t) == 0);
		memset(allocations[i], (int)(i + 1), sizes[i]);
	}

	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		unsigned char *bytes = allocations[i];
		for (size_t j = 0; j < sizes[i]; j++)
			CHECK(bytes[j] == (unsigned char)(i + 1));
	}

	CHECK(check_invariants("alignment/writable memory"));
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
		afree(allocations[i]);

	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
	CHECK(check_invariants("alignment cleanup"));
}

static void test_reuse_and_coalescing_bookkeeping(void) {
	void *first = aalloc(64);
	void *second = aalloc(128);
	void *guard = aalloc(64);
	CHECK(first != NULL && second != NULL && guard != NULL);
	if (first == NULL || second == NULL || guard == NULL)
		return;

	afree(first);
	void *reused = aalloc(64);
	CHECK(reused == first);
	afree(reused);
	afree(second);
	CHECK(check_invariants("adjacent free blocks"));

	void *another = aalloc(160);
	CHECK(another != NULL);
	CHECK(check_invariants("after allocation following coalescing"));
	if (another != NULL)
		afree(another);
	afree(guard);

	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
	CHECK(check_invariants("reuse/coalescing cleanup"));
}

static void test_heap_boundaries_and_failure(void) {
	size_t largest = HEAPSIZE - HEAP_HEADER_SIZE - BLOCK_HEADER_SIZE;
	size_t alignment = _Alignof(max_align_t);
	largest -= largest % alignment;

	void *max_allocation = aalloc(largest);
	CHECK(max_allocation != NULL);
	if (max_allocation != NULL)
		memset(max_allocation, 0x5a, largest);
	CHECK(check_invariants("largest allocation"));
	if (max_allocation != NULL)
		afree(max_allocation);
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);

	CHECK(aalloc(largest + alignment) == NULL);
	CHECK(aalloc(SIZE_MAX) == NULL);
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
	CHECK(check_invariants("oversized allocation"));
}

static void test_multiple_heap_unlinking(void) {
	void *older_allocation = aalloc(3000);
	struct s_heap *older_heap = HEAD;
	void *newer_allocation = aalloc(3000);
	struct s_heap *newer_heap = HEAD;

	CHECK(older_allocation != NULL && newer_allocation != NULL);
	CHECK(older_heap != NULL && newer_heap != NULL);
	if (older_allocation == NULL || newer_allocation == NULL ||
		older_heap == NULL || newer_heap == NULL)
		return;

	CHECK(newer_heap != older_heap);
	CHECK(newer_heap->next == older_heap);
	CHECK(heapcount == 2);
	CHECK(check_invariants("two heaps"));

	afree(older_allocation);
	CHECK(HEAD == newer_heap);
	CHECK(newer_heap->next == NULL);
	CHECK(heapcount == 1);
	CHECK(check_invariants("unlink non-head heap"));

	afree(newer_allocation);
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
	CHECK(check_invariants("multiple heap cleanup"));
}

int main(void) {
	test_empty_and_null_free();
	test_alignment_and_writable_memory();
	test_reuse_and_coalescing_bookkeeping();
	test_heap_boundaries_and_failure();
	test_multiple_heap_unlinking();

	if (failures != 0) {
		fprintf(stderr, "%d allocator checks failed\n", failures);
		return 1;
	}

	puts("All allocator tests passed.");
	return 0;
}

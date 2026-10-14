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

static int check_heap_ring(size_t expected_count, const char *test_name) {
	if (expected_count == 0) {
		if (HEAD != NULL || heapcount != 0) {
			fprintf(stderr, "%s: expected an empty heap ring\n", test_name);
			return 0;
		}
		return 1;
	}

	if (HEAD == NULL || heapcount != expected_count) {
		fprintf(stderr, "%s: unexpected heap count or null HEAD\n", test_name);
		return 0;
	}

	size_t count = 0;
	struct s_heap *h = HEAD;
	do {
		if (h->next == NULL || h->prev == NULL ||
			h->next->prev != h || h->prev->next != h) {
			fprintf(stderr, "%s: broken circular heap links\n", test_name);
			return 0;
		}
		count++;
		if (count > expected_count) {
			fprintf(stderr, "%s: heap ring does not close\n", test_name);
			return 0;
		}
		h = h->next;
	} while (h != HEAD);

	if (count != expected_count) {
		fprintf(stderr, "%s: ring has %zu heaps, expected %zu\n",
			test_name, count, expected_count);
		return 0;
	}
	return 1;
}

static void test_round_robin_and_heap_removal(void) {
	void *large_a = aalloc(3000);
	CHECK(large_a != NULL);
	if (large_a == NULL)
		return;
	struct s_heap *heap_a = getheap(
		(struct s_block *)((char *)large_a - BLOCK_HEADER_SIZE));
	void *large_b = aalloc(3000);
	CHECK(large_b != NULL);
	if (large_b == NULL) {
		afree(large_a);
		return;
	}
	struct s_heap *heap_b = getheap(
		(struct s_block *)((char *)large_b - BLOCK_HEADER_SIZE));

	CHECK(heap_a != heap_b);
	CHECK(check_heap_ring(2, "two heaps"));
	if (heap_a == heap_b)
		return;

	void *small_a = aalloc(64);
	void *small_b = aalloc(64);
	CHECK(small_a != NULL);
	if (small_a == NULL)
		return;
	CHECK(small_b != NULL);
	if (small_b == NULL) {
		afree(small_a);
		return;
	}

	struct s_heap *small_heap_a = getheap(
		(struct s_block *)((char *)small_a - BLOCK_HEADER_SIZE));
	struct s_heap *small_heap_b = getheap(
		(struct s_block *)((char *)small_b - BLOCK_HEADER_SIZE));
	CHECK(small_heap_a == heap_a);
	CHECK(small_heap_b == heap_b);
	CHECK(check_heap_ring(2, "round-robin allocations"));

	void *small_c = aalloc(64);
	CHECK(small_c != NULL);
	if (small_c == NULL)
		return;
	struct s_heap *small_heap_c = getheap(
		(struct s_block *)((char *)small_c - BLOCK_HEADER_SIZE));
	CHECK(small_heap_c == heap_a);

	afree(large_a);
	afree(small_a);
	afree(small_c);
	CHECK(check_heap_ring(1, "remove heap not at HEAD"));
	CHECK(HEAD == heap_b);
	afree(large_b);
	afree(small_b);
	CHECK(check_heap_ring(0, "after releasing all heaps"));
}

static void test_coalescing_in_later_heap(void) {
	void *large_a = aalloc(3000);
	CHECK(large_a != NULL);
	if (large_a == NULL)
		return;
	struct s_heap *heap_a = getheap(
		(struct s_block *)((char *)large_a - BLOCK_HEADER_SIZE));

	void *large_b = aalloc(3000);
	CHECK(large_b != NULL);
	if (large_b == NULL)
		return;
	struct s_heap *heap_b = getheap(
		(struct s_block *)((char *)large_b - BLOCK_HEADER_SIZE));

	void *a1 = aalloc(160);
	void *b1 = aalloc(160);
	void *a2 = aalloc(160);
	void *b2 = aalloc(160);
	void *a3 = aalloc(160);
	void *b3 = aalloc(160);
	void *a4 = aalloc(160);
	void *b4 = aalloc(160);
	CHECK(a1 != NULL && b1 != NULL && a2 != NULL && b2 != NULL);
	CHECK(a3 != NULL && b3 != NULL && a4 != NULL && b4 != NULL);
	if (a1 == NULL || b1 == NULL || a2 == NULL || b2 == NULL ||
		a3 == NULL || b3 == NULL || a4 == NULL || b4 == NULL)
		return;

	CHECK(getheap((struct s_block *)((char *)a1 - BLOCK_HEADER_SIZE)) == heap_a);
	CHECK(getheap((struct s_block *)((char *)b1 - BLOCK_HEADER_SIZE)) == heap_b);
	CHECK(check_heap_ring(2, "before later-heap coalescing"));

	afree(b1);
	afree(b2);
	void *merged = aalloc(304);
	CHECK(merged != NULL);
	if (merged != NULL) {
		struct s_block *allocated = (struct s_block *)
			((char *)merged - BLOCK_HEADER_SIZE);
		char *lower = (char *)b1 < (char *)b2 ? (char *)b1 : (char *)b2;
		char *upper = (char *)b1 > (char *)b2 ? (char *)b1 : (char *)b2;
		CHECK((char *)merged >= lower);
		CHECK((char *)merged < upper);
		CHECK(allocated->freed == 0);
		CHECK(allocated->size >= 304);
		CHECK(getheap((struct s_block *)((char *)merged - BLOCK_HEADER_SIZE)) == heap_b);
	}

	afree(large_a);
	afree(a1);
	afree(a2);
	afree(a3);
	afree(a4);
	CHECK(check_heap_ring(1, "after removing first heap"));

	afree(large_b);
	afree(merged);
	afree(b3);
	afree(b4);
	CHECK(check_heap_ring(0, "after later-heap coalescing cleanup"));
}

int main(void) {
	test_round_robin_and_heap_removal();
	test_coalescing_in_later_heap();

	if (failures != 0) {
		fprintf(stderr, "%d heap checks failed\n", failures);
		return 1;
	}
	puts("Circular heap tests passed.");
	return 0;
}

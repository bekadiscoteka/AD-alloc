#include <aalloc_internal.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int failures;

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
			__FILE__, __LINE__, #condition); \
		failures++; \
	} \
} while (0)

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
		afree(allocations[i]);
	}
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
}

static void test_heap_boundaries_and_failure(void) {
	size_t largest = HEAPSIZE - HEAP_HEADER_SIZE - BLOCK_HEADER_SIZE;
	size_t alignment = _Alignof(max_align_t);
	largest -= largest % alignment;

	void *near_max_allocation = aalloc(largest - alignment);
	CHECK(near_max_allocation != NULL);
	if (near_max_allocation != NULL)
		afree(near_max_allocation);
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);

	void *max_allocation = aalloc(largest);
	CHECK(max_allocation != NULL);
	if (max_allocation != NULL) {
		memset(max_allocation, 0x5a, largest);
		afree(max_allocation);
	}
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);

	CHECK(aalloc(largest + alignment) == NULL);
	CHECK(aalloc(SIZE_MAX) == NULL);
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
}

static void test_empty_and_null_free(void) {
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
	afree(NULL);
	CHECK(HEAD == NULL);
	CHECK(heapcount == 0);
}

int main(void) {
	test_empty_and_null_free();
	test_alignment_and_writable_memory();
	test_heap_boundaries_and_failure();

	if (failures != 0) {
		fprintf(stderr, "%d alignment checks failed\n", failures);
		return 1;
	}
	puts("Alignment and boundary tests passed.");
	return 0;
}

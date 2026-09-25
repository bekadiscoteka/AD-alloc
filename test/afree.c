#include <stdio.h>

#define DEBUG
#define TEST
#include "aalloc.h"

int main(void) {
	int allocsize = 1000;
	void* p = aalloc(allocsize);
	printf(	"allocated: %d\n"
			"heapcount: %ld\n", allocsize, heapcount);
	heapinfo(HEAD);
	afree(p);
	printf("\n\nfree the memory\n\n");
	heapinfo(HEAD);
	return 0;
}

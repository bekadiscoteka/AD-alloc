#define DEBUG

#include <aalloc.h>
#include <stdio.h>
#include <string.h>

void headersinfo(void) {
	printf("heapcount %zu\n", heapcount);
	struct s_heap* p = HEAD; 
	while (p != NULL) {
		heapinfo(p);
		p = p->next;
	}
}

int main(void) {
	char *basic  = aalloc(4);
	afree(basic);
	char *medium = aalloc(100); 
	afree(medium);
	char *large  = aalloc(2000);
	afree(large);
	char *largest = aalloc(3000);
	afree(largest);

	headersinfo();
		
	return 0;
}

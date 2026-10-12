#define DEBUG

#include <aalloc.h>
#include <stdio.h>
#include <string.h>

void headersinfo(void) {
	printf("heapcount %d\n", heapcount);
	struct s_heap* p = HEAD; 
	while (p != NULL) {
		heapinfo(p);
		p = p->next;
	}
}

int main(void) {
	char *mem = aalloc(4);

	headersinfo();
		
	afree(mem);
	return 0;
}

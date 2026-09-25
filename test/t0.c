
#define TEST
#define DEBUG

#include "aalloc.h"
#include <stdio.h>
#include <string.h>


int main(void) {
	size_t req = (HEAPSIZE - ( (HEAPSIZE/2) + sizeof(struct s_heap) + sizeof(struct s_block) ) - sizeof(struct s_block));
	char *name0 = aalloc(HEAPSIZE / 2);
	char *name1 = aalloc(req);
	printf("req=%zu\n", req);
	strcpy(name0, "ye");
	strcpy(name1, "heaplkjlsdjf");
	printf("%s\n%s", name0, name1);
	printf("HEAD=%p heapnum=%d blocknum=%d splitnum=%d\n", HEAD, heapnum, blocknum, splitnum);
	
	for (struct s_heap *p=HEAD; p != NULL; p=p->next) 
		printf("%zu ", p->free_size);	
		
	printf("\n");	
	afree(name1);
	afree(name0);
	
	printf("%zu", HEAD->free_size);

	return 0;
}

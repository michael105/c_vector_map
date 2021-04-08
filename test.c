#include <stdio.h>
#include <sys/mman.h>

#include "vector_map.h"





int main(int argc, char **argv){

	VM* vm = vm_new();


	vm_add(1,"First element\n",vm);
	vm_add(2,"Second element\n",vm);

	char buf[64];
	strcpy(buf,"Element Nr.  \n");

	for ( int i=3; i<10; i++ ){
		buf[12] = i + '0';
		vm_add(i,buf,vm);
	}
	
	printf("fetch, 5: %s",vm_get(5,vm));

	for ( int i=10; i<400; i++ ){
		sprintf( buf, "Element (extending the medium size): %d\n", i);
		vm_add(i,buf,vm);
	}


	printf("fetch: %s",vm_get(15,vm));
	printf("fetch: %s",vm_get(155,vm));
	printf("fetch: %s",vm_get(355,vm));
	printf("fetch: %s",vm_get(100,vm));
	printf("fetch: %s",vm_get(224,vm));

	return(0);
}

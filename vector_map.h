#ifndef vector_map_h
#define vector_map_h

#ifndef PAGESIZE
#ifndef PAGESIZE_EXEC
#include <asm-generic/param.h>
#endif
#define PAGESIZE EXEC_PAGESIZE
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif

#ifndef MLIB
#include <err.h>
#include <string.h>
#endif

// how many directories per (preallocated) page
// -> (PAGESIZE/16) = 256 for 4kB pages.
// each path can be in the medium (PAGESIZE-4*256 - 20)/256 Bytes.
// The notify dirlist ist dynamically grown.
#define VM_ELEMENTS (PAGESIZE/16)



#ifndef OPTFENCE
// prevent gcc to optimize away registers and variables
//
// the macro OPTFENCE(...) can be invoked with any parameter.
// The parameters will get calculated, even if gcc doesn't recognize
// the use of the parameters, e.g. cause they are needed for an inlined asm syscall.
//
// The macro translates to an asm jmp and a function call to the function 
// opt_fence, which is defined with the attribute "noipa" -
// (the compiler "forgets" the function body, so gcc is forced
// to generate all arguments for the function)
// The generated asm jump hops over the call to the function,
// but this gcc doesn't recognize.
//
// This generates some overhead, 
// (a few (never reached) bytes for setting up the function call, and the jmp)
// but I didn't find any other solution,
// which gcc wouldn't cut for optimizations from time to time.
// (volatile, volatile asm, optimize attributes, 
// andsoon have all shown up to be unreliable - sometimes(!)).
//
// Had some fun debugging these bugs, which naturally showed up only sometimes.
// (Many syscalls also work with scrambled arguments..)
// And, I believe it IS a compiler bug. 
// Volatile should be volatile for sure, not only sometimes.
// I mean, why the heck do I write volatile?? 
void __attribute__((noipa,cold,naked)) opt_fence(void*p,...){}
#define _optjmp(a,b) asm( a "OPTFENCE_"#b )
#define _optlabel(a) asm( "OPTFENCE_" #a ":" )
#define __optfence(a,...) _optjmp("jmp ", a ); opt_fence(__VA_ARGS__); _optlabel(a)
#define OPTFENCE(...) __optfence(__COUNTER__,__VA_ARGS__)
#endif


#ifdef DEBUG
#ifndef dbg
#define dbg(msg) puts(msg)
#define dbgf(fmt,...) printf(fmt,__VA_ARGS__)
#define dbgs(msg,msg2) puts(msg);puts(msg2)
#endif
#endif

#ifndef dbg
#define dbg(msg) {}
#define dbgf(fmt,...) {}
#define dbgs(msg,msg2) {}
#endif


#ifndef RED
#define RED "\e[31m"
#define NORM "\e[37m"
#define CYAN "\e[36m"
#endif

// Avoids with uint32 the penalty of unaligned memory access
typedef unsigned int p_rel;

static inline char* _getaddr( p_rel *i, p_rel addr ){
	return((char*)i + addr );
}

static inline p_rel _setaddr( p_rel *i, char *p ){
	return( *i = (p - (char*)i ));
}

// translate a relative pointer to an absolute address
#define getaddr( addr ) _getaddr( &addr, addr )

// store the absolute pointer as relative address in relative_p
#define setaddr( relative_p, pointer ) _setaddr( &relative_p, pointer )


typedef struct _VM{
	p_rel array[VM_ELEMENTS];
	struct _VM* next;
	int max;
	int subtract;
	// dynamic string section starts here
	char stringsstart[0];
} VM;



VM* vm_new(){
	VM *node = mmap( 0, PAGESIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if( node < 0 ) 
		err(ENOMEM, "Couldn't map memory");

	node->max = VM_ELEMENTS;
	node->next = 0; // for clarity
	setaddr( node->array[0], node->stringsstart );
	// prevent gcc of optimizing assignments out
	OPTFENCE(node);
	return(node);
}


const char* vm_get( int num, VM *nod ){
	num-=nod->subtract;

	if ( num<0 ) // happens for inotify_rm_watch events
		return(0); // closing the inotify fd didn't work out here
	// so every watch needs to be removed when reloading the config :/

	while( ( num >= nod->max-1) ){ // or addr > map
		num-= (nod->max-1);
		if ( nod->next ){
			nod = nod->next;
		} else { // append
			//die(-14,"No such ino_dir");
			return(0);
		}
	}

	return( getaddr( nod->array[num] ) );
}

void vm_destroy( VM *nod, void*(callback)(const char*e)){
	const char *e;
	for (int a = nod->subtract;(e = vm_get(a,nod));a++){
		dbgf(CYAN "remove: %d\n" NORM , a );
		//inotify_rm_watch(nfd,a);
		callback(e);
	}
	do {
		dbg("unmap");
		char *tmp = (char*) nod;
		nod = nod->next;
		munmap( tmp, PAGESIZE );
	} while ( nod );
}

VM *vm_addmapping( VM* nod ){
	dbg("addmapping");
	nod->next = (VM*)mmap( 0, PAGESIZE, PROT_READ|PROT_WRITE, 
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	nod = nod->next;
	nod->max = VM_ELEMENTS;
	setaddr( nod->array[0], nod->stringsstart );
	return(nod);
}

// append a path. num MUST be sequential
void vm_add( int num, const char* path, VM *nod ){

	dbgf("num: %d - %s\n",num,path);

	if ( !nod->subtract ){
		nod->subtract = num;
	}
	num-= nod->subtract;

	dbgf("num: %d\n",num);
	while( ( num >= nod->max-1) ){
		num-=nod->max-1;
		if ( nod->next ){
			nod = nod->next;
		} else { // append
			dbg(RED"append mmap"NORM);
			//goto newmap;
			nod = vm_addmapping(nod);
			break;
		}
	}

	dbgf("num: %d\n",num);
	if ( nod->array[num+1] )
		err(0,"ino_dirs: num %d already used!\n",num);
	dbg("0.1");

	if ( (int)( (nod->array[num] + 
					( sizeof(p_rel)*(VM_ELEMENTS - num) ) + 
					// = rel pos of path[num] to stringsstart 
					strlen(path))) >= PAGESIZE ) {  
		dbg("1");
		nod->max = num+1; // addr > map
		num = 0;
		nod = vm_addmapping(nod);
	}

	dbgf("pos: %d   len: %d\n", nod->array[num], strlen(path) );
	dbgf("s1: %d   s2: %d\n", sizeof(VM), sizeof(p_rel) );

	dbg("copy");
	char *p = stpcpy( getaddr( nod->array[num] ), path );
	p++;
	setaddr(nod->array[num+1],p);
	dbgs( "appended: ", getaddr( nod->array[num] ) );
}


#endif

## vector_map


This is a lowlevel data structure, 
which might be best decribed as a linked skiplist of mapped vectors,
using mmap for memory allocations, and managing the memory in blocks
of a pagesize (4kB at linux x64).

In its current form it maps variable strings to integers;
the integers have to be inserted sequentially, 
but do not neccessarily need to start with 1.

It is optimized for sequentially appending elements and ramDom read access,
with a known medium length of the elements. *err* Elements have to be appended
necessarily in sequential order, and the structure is optimized for random access.
On missing numbers in the sequence an error is raised.

Neither insertion nor deletion of elements is possible,
and you have to supply the integer keys in sequential order for yourself.
(I have to admit, this data structure is that special, there might not be that many usecases..
Eventually mapping inodes to filenames)

All mapped pointers are converted to relative pointers with 4 Bytes length.
I'm going to convert the internal pointers to relative pointers as well,
so it would be possible to mmap the structure in it's whole to a file.


However, I wrote the structure for an udevd daemon, watching the mounted /dev directory
with inotify events - there you have to keep track of the inotify id's,
and map them to the watched paths. 

I leave it as it is for now, the structure is quite optimized for its special purpose;
also with it's bounds and alignment to the pagesize, and going to add another
optionally file backed and more dynamic structure later.


The basic memory layout looks like this:

```
list
{ 
	listelement (One memory page)
	array[] with relative pointers to
			[ elements of variable len ]
}
```


---

### Notes

I push this online as it is.

It is highly specialized for its very own usecase - keeping track of the inotify id's,
you get for each inotify watch. There you have to store for every inotify id the according path.

Therefore having the knowledge, of how long the strings added are in the medium, 
and how many there are (most possibly) going to be added, but having the possibility
to allocate dynamically more memory.

It wouldn't be a problem to e.g. change the blocksize of the managed memory blocks;
change the count of elements per block dynamically, or use the spare memory areas,
left over at the end of a block. However, this would add some overhead again;
and I didn't need it in the udevd daemon I wrote.
There I did focus at the most minimal resource usage possible.

Competitive benchmarks would be quite interesting.

I'm not really sure, whether this structure in it's current form has very many usecases.
However, changing e.g. the number of elements per page dynamically, or preallocating more than one page would be trivial. 

I guess I'm going to add a changed version of this to a malloc, I'm planning.



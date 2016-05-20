PROJECT#2- DIY ALLOCATOR 
											NAME- ANAMIKA MODI 
											ID-201401045
											TEAM NAME -Finesse
								
1.PROBLEM DESCRIPTION -
The project aims at creation of a dynamic memory allocator involving implemenation of malloc,free and realloc.It aims at improving the memory utilisation and throughput of the allocator , so as to achieve better performance. 
The given allocator generated performance index of 30/100 done via implicit free list.

2.DESIGN OF THE ALLOCATOR -
-> Explicit free list for memory allocation
-> First fit Scanning alogrithm

The explicit free list is a doubly linked list of memory blocks which contain only the free blocks.Thus while allocating memory,the free list is searched and block with appropriate size is alloacted.

The blocks are designed as follows-
->FREE BLOCK - HEADER|PREV POINTER|NEXT POINTER|---OLD DATA----|FOOTER
->ALLOCATED BLOCK - HEADER|---DATA----|FOOTER

In the alloactor the minimum block size is taken to be 32 i.e. 2*DSIZE considering 64 bit machines as minimum block will include a header (WSIZE) and a footer(WSIZE) and a prev and next pointer (WSIZE each) .

Intially heap looks like -
|padding|prologue header|prologue footer|epilogue header|

3.IMPLEMENTATION OF THE ALLOCATOR-
Functions used-
mm_init()-intialising heap
mm_malloc()- malloc function
mm_free() - for free 
mm_realloc() - Reallocation
place() - for splitting the extra memory into a new block and uptading allocated bits 
coalesce() - for combining adjacent free blocks
extend_heap()- for extending the heap with a given size
removeblock() - for removing a block from the free list
insertblock()-for inserting a block to free list

MALLOCING -  Allocates a block with at least desired size of payload via find_fit (first fit scanning alogrithm),after which it places it , i.e. if there is some extra memory that can be made into another block then splitting occurs,if block is not allocated then it extends the heap and places the block.

FREEING - the allocation bit of the block is changed by manupulating the header and footer content of the block and then this block is added to the free list ,via a call to coalesce which checks if any adjacent blocks are free then combines them and adds to the free list.

REALLOCING -  If desired size is zero, frees the block and returns NULL.  If the block is already a block with at least the desired no of bytes of payload, then block itself is returned.Otherwise, if the next  block  is free and the total size of the next and current block is greater than or equal to desired size then current and next are made into one block and this new block is returned or else a new  block is allocated and the contents of the old block are copied to that new block.

4.PERFORMANCE - The performance index has increased upto 89/100 via this approach.Here searching for the free block has become easier as all the free blocks are now in a list thus the search time for allocation has descreased to no of free blocks as opposed to total no of blocks.Also the optimisation in realloc where a check on next block is implemented, makes the performance better.










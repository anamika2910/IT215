/*
 * Simple, 32-bit and 64-bit clean allocator based on an implicit free list,
 * first fit placement, and boundary tag coalescing, as described in the
 * CS:APP2e text.  Blocks are aligned to double-word boundaries.  This
 * yields 8-byte aligned blocks on a 32-bit processor, and 16-byte aligned
 * blocks on a 64-bit processor.  However, 16-byte alignment is stricter
 * than necessary; the assignment only requires 8-byte alignment.  The
 * minimum block size is four words.
 *
 * This allocator uses the size of a pointer, e.g., sizeof(void *), to
 * define the size of a word.  This allocator also uses the standard
 * type uintptr_t to define unsigned integers that are the same size
 * as a pointer, i.e., sizeof(uintptr_t) == sizeof(void *).
 */

/*The writeup.txt includes an explation of implementation and design for the allocator
* Done via Explicit free list.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
	/* Team name */
	"Finesse",
	/* First member's full name */
	"Anamika Modi",
	/* First member's email address */
	"201401045@daiict.ac.in",
	/* Second member's full name (leave blank if none) */
	"-",
	/* Second member's email address (leave blank if none) */
	"-"
};

/* Basic constants and macros: */
#define WSIZE      sizeof(void *) /* Word and header/footer size (bytes) */
#define DSIZE      (2 * WSIZE)    /* Doubleword size (bytes) */
#define CHUNKSIZE  (1 << 12)      /* Extend heap by this amount (bytes) */

#define MAX(x, y)  ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word. */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p, val)  (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p. */
#define GET_SIZE(p)   (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer. */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks. */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, generate next and previous pointers. */
#define NEXT_POINTER(bp) (*((void**)(bp+ WSIZE)))
#define PREV_POINTER(bp)  *((void**)(bp))

/* Global variables: */
static char *heap_listp; /* Pointer to first block */
static char *head;    	/*Pointer to the head of the free list */

/* Function prototypes for internal helper routines: */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/*Function for manupulating the doubly free linked list*/
static void removeblock(void *bp);
static void insertblock(void *bp);

/* Function prototypes for heap consistency checker routines: */
static void checkblock(void *bp);
static void checkheap(bool verbose);
static void printblock(void *bp);
/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Initialize the memory manager.  Returns 0 if the memory manager was
 *   successfully initialized and -1 otherwise.
 */
int mm_init(void)
{
	/* Create the initial empty heap. */
	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
		return (-1);
	PUT(heap_listp, 0);                            /* Alignment padding */
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
	/*heap_listp points at the footer of the prologue*/
	heap_listp += (2 * WSIZE);
	/*head points at the start of the first block intially */
	head=heap_listp+DSIZE;
	/* Extend the empty heap with a free block of CHUNKSIZE bytes. */
	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
		return (-1);
	return (0);
}

/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Allocate a block with at least "size" bytes of payload, unless "size" is
 *   zero.  Returns the address of this block if the allocation was successful
 	 or less it extends the heap and places the block.
 */
void *mm_malloc(size_t size)
{
	size_t asize;      /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	void *bp;			/*Pointer to return suitable block*/

	/* Ignore spurious requests. */
	if (size == 0)
		return (NULL);

	/* Adjust block size to include overhead and alignment reqs. as minimum size of the block is 32 */
	if (size <= 2*WSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
/* Search the free list for a fit. */
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return (bp);
	}
	/* No fit found.  Get more memory and place the block. */
	extendsize = MAX(asize,2*CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return (NULL);
	place(bp, asize);
	return (bp);
}

/*
 * Requires:
 *   "bp" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Free a block.
 */
void mm_free(void *bp)
{
	size_t size=0;
	/* Ignore spurious requests. */
	if (bp == NULL)
		return;
	/* Free and coalesce the block. */
	size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	/*Making next and prev pointer of the freed block null */
	NEXT_POINTER(bp)=NULL;
	PREV_POINTER(bp)=NULL;
	PUT(FTRP(bp), PACK(size, 0));
	/*After making next and prev pointers null to get rid of
	* any previous allocations
	*and then coalesce so as to check if any free bloacks adjacent
	*to freed block and insertion to free list
	*/
	coalesce(bp);
}

/*
 * Requires:
 *   "ptr" is either the address of an allocated block or NULL and
 *	  size is the newsize of the block to be allocated.
 *
 * Effects:
 *   Reallocates the block "ptr" to a block with at least "size" bytes of
 *   payload, unless "size" is zero.  If "size" is zero, frees the block
 *   "ptr" and returns NULL.  If the block "ptr" is already a block with at
 *   least "size" bytes of payload, then "ptr" is returned.Otherwise, if the next
 *   block of "ptr" is free and the total size of the next and "ptr" is greater than or equal to
 *   "size" then ptr and next are made into one block and this new block is returned or else a new
 *   block is allocated and the contents of the old block
 *   "ptr" are copied to that new block.  Returns the address of this newly
 *   allocated block.
 */
void *mm_realloc(void *ptr, size_t size)
{
	if((int)size == 0){
    	mm_free(ptr);
    	return NULL;
  	}
  	/*since size is the size of the payload hence adding the size of header and footer*/
    size_t new_size = size + 2 * WSIZE;
    /*old_size will have the size of "ptr"*/
   	size_t old_size = GET_SIZE(HDRP(ptr));
     /*if new_size is less than old_size then we just return bp */
    if(new_size <= old_size){
         return ptr;
    }
    /*if newsize is greater than oldsize */
    else {
    	/*alloaction bit of the next block of "ptr"*/
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));

        /* if next block is free and the total size of "ptr" and next combined is greater than
         * or equal the new size then we only need to combine both the blocks,via this approach the
         * the data neednot be copied and  pointer to the same block ie. "ptr" is returned but now combined with
         * the next block , so as to satisfy the "size" requirement */

        if(!next_alloc && GET_SIZE(HDRP(NEXT_BLKP(ptr)))>= (new_size-old_size)){
          	/*Calculating the total size of next and "ptr" combined */
          	size_t final_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
          	/* Since the next block is combined with "ptr" ,the free next block
          	 * has to be removed from the free list  */
            removeblock(NEXT_BLKP(ptr));
            /*changing the hdr and ftr so as to combine next and "ptr"*/
            PUT(HDRP(ptr), PACK(final_size, 1));
            PUT(FTRP(ptr), PACK(final_size, 1));
            return ptr;
        }
          /* as next block wasn't free thus a new fresh chunk of memory of size "new_size" is
           * alloacted via a call to malloc */
          else {
            void *new_ptr = mm_malloc(new_size);
            place(new_ptr,new_size);
            memcpy(new_ptr, ptr, new_size);
            mm_free(ptr);
            return new_ptr;
        }
      }
    return NULL;
}

/*
 * The following routines are internal helper routines.r
 */

/*
 * Requires:
 *   "bp" is the address of a newly freed block.
 *
 * Effects:
 *   Perform boundary tag coalescing.Remove the next or prev block or both if they are free
 *   and coalesce and then add the new coalesced block to the free list .
 *   Return the address of the coalesced block.
 */
static void *coalesce(void *bp)
{
	bool prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	bool next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
	/* Case 1 - if neither prev nor next is free then add bp in free list an return free */
	if (prev_alloc && next_alloc) {
		insertblock(bp);
		return (bp);
	}
	/* Case 2 - if prev is allocated but next is free then remove next from the list ,
	*  colaesce and add the newblock to free list  */
	else if (prev_alloc && !next_alloc) {
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		removeblock(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	/* Case 3 - if next is allocated but prev is free then remove prev from the list ,
	*  colaesce and add the newblock to free list  */
	else if (!prev_alloc && next_alloc) {
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		removeblock(PREV_BLKP(bp));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	/* Case 4 - if prev and next both are free then remove next and prev  from the list ,
	*  colaesce and add the newblock to free list  */
	else {
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
		    GET_SIZE(FTRP(NEXT_BLKP(bp)));
		removeblock(PREV_BLKP(bp));
		removeblock(NEXT_BLKP(bp));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	insertblock(bp);
	return (bp);
}

/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Extend the heap with a free block and return that block's address.
 */
static void *extend_heap(size_t words)
{
	void *bp;
	size_t size;
	/* Allocate an even number of words to maintain alignment. */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == (void *)-1)
		return (NULL);
	/* Initialize free block header/footer and the epilogue header. */
	PUT(HDRP(bp), PACK(size, 0));        /* Free block header */
	PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
	/*Intialize the next and prev pointer of the extended free block to null*/
	NEXT_POINTER(bp)=NULL;
	PREV_POINTER(bp)=NULL;
	/* Coalesce if the previous block was free. */
	return (coalesce(bp));
}

/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Find a fit for a block with "asize" bytes from the free list. Returns that block's address
 *   or NULL if no suitable block was found.
 */
static void *find_fit(size_t asize)
{
	void *bp;
	/*if head is null that indicates there are no free block in the heap and thus return null*/
	if(head==NULL){
	return NULL;
	}
	/* Search for the first fit from the free list ,for scanning the free list the
	* for loop starts at head and goes to the next pointer everytime and stops if reaches the end of the
	free list */
	for (bp = head;((bp!=NULL)); bp = NEXT_POINTER(bp)) {
		if ((asize <= GET_SIZE(HDRP(bp)))){
			return (bp);
		}
	}
	/* No fit was found. */
	return (NULL);
}

/*
 * Requires:
 *   "bp" is the address of a free block that is at least "asize" bytes.
 *
 * Effects:
 *   Place a block of "asize" bytes at the start of the free block "bp" and
 *   split that block if the remainder would be at least the minimum block
 *   size(for 64-bit system considered as 2*DSIZE),before splitting the free block bp is removed from
 *   the free list and the next block is sent for coalescing.
 */
static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));
	/*if splitting is possible*/
	if ((csize - asize) >= (2 * DSIZE)) {
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		/*remove bp sice it is allocated now */
		removeblock(bp);
		/*split*/
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
		NEXT_POINTER(bp)=NULL;
		PREV_POINTER(bp)=NULL;
		coalesce(bp);
	}
	/*no splitting*/
	else {
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		removeblock(bp);
	}
}

/*
 * The remaining routines are heap consistency checker routines.
 */

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Perform a minimal check on the block "bp".
 */
static void checkblock(void *bp)
{

	if ((uintptr_t)bp % DSIZE)
		printf("Error: %p is not doubleword aligned\n", bp);
	if (GET(HDRP(bp)) != GET(FTRP(bp)))
		printf("Error: header does not match footer\n");

}

/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Perform a check of the heap for consistency.
 *   Checks for -
 *   -> the allocation bit and size of the prologue.
 *   -> the consistency of every block in the heap
 *   -> the allocation bit and size of the epilogue
 *   -> if every free block in free list is consistent(i.e. has a valid address etc.)
 *   -> if an allocated block is in the free list
 *   -> if some blocks have escaped coalesced
 *   -> if some free block is not added to the free list
 */
void checkheap(bool verbose)
{
	void *bp;
	/*print the start of the heap */
	if (verbose)
		printf("Heap (%p):\n", heap_listp);
	/*checking for prologue's size and allocation bit*/
	if (GET_SIZE(HDRP(heap_listp)) != DSIZE ||
	    !GET_ALLOC(HDRP(heap_listp)))
		printf("Bad prologue header\n");
	checkblock(heap_listp);
	/*check for consistency of every block in heap */
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (verbose)
			printblock(bp);
		checkblock(bp);
	}
	/*print the epilogue */
	if (verbose)
		printblock(bp);
	/*checking for prologue's size and allocation bit*/
	if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
		printf("Bad epilogue header\n");
	/*check for consistency of every free block in free list and if
	* any allocated block is added in the free list */
	for (bp = head;(bp!=NULL)&&(GET_SIZE(HDRP(bp)) > 0); bp = NEXT_POINTER(bp)) {
		/*allocated block*/
		if(GET_ALLOC(HDRP(bp))!=0){
			printf("Error : Allocated Block in free list\n");
			printblock(bp);
		}
		/*consistency of free block*/
		checkblock(bp);
	}
	/* check if any blocks escaped coalescing by checking
	* if any block in the heap as free prev or next pointer.
	*/
	for (bp = heap_listp;(GET_SIZE(HDRP(bp)) > 0); bp = NEXT_BLKP(bp)) {
		/*checks if bp is not allocated then goes to prev and next blocks and checks if free*/
		 if(GET_ALLOC(HDRP(bp))==0){
			bool prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
			bool next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
			if(prev_alloc&&!next_alloc){
				printf("block at %p escaped coalescing with the next block\n", bp);
				printblock(bp);
				printblock(NEXT_BLKP(bp));
			}
			else if(!prev_alloc&&next_alloc){
				printf("block at %p escaped coalescing with the previous block\n", bp);
				printblock(bp);
				printblock(PREV_BLKP(bp));
			}
			else if(!prev_alloc&&!next_alloc){
				printf("block at %p escaped coalescing with the next and previous block\n", bp);
				printblock(bp);
				printblock(PREV_BLKP(bp));
				printblock(NEXT_BLKP(bp));
			}
		}
	}
	/* checks if every free block in the heap is added to the free list */
	for (bp = heap_listp;(GET_SIZE(HDRP(bp)) > 0)&&(GET_ALLOC(HDRP(bp))==0); bp = NEXT_BLKP(bp)) {
		int flag=0;/*if 0 indicates not added */
			void *temp;
			for (temp = head;((temp!=NULL)&&(GET_SIZE(HDRP(bp)) > 0)); temp= NEXT_POINTER(temp)) {
				if(head==NULL){
					flag=0;
				}
			/*comapring addresses and checking*/
		      if(GET(bp)==GET(temp)){
		      	flag=1;
		      	break;
			}
				}
			if(flag==0)
				printf("ERROR as  block at address %p not added in the free list\n",bp);
	}
}

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Print the block "bp".
 */
static void printblock(void *bp)
{
	bool halloc, falloc;
	size_t hsize, fsize;

	checkheap(false);
	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));

	if (hsize == 0) {
		printf("%p: end of heap\n", bp);
		return;
	}

	printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp,
	    hsize, (halloc ? 'a' : 'f'),
	    fsize, (falloc ? 'a' : 'f'));
}
/*
Requires:
a pointer "bp" to the block to be removed

Effects:
removes the block  with address "bp" from the free list.
*/
static void removeblock(void *bp){
	void *prev=PREV_POINTER(bp);
	void *nextp=NEXT_POINTER(bp);
	if(prev!=NULL){
		NEXT_POINTER(prev)=nextp;
	}
	else{
		head=(char*)NEXT_POINTER(bp);
	}
	if(nextp!=NULL)
		PREV_POINTER(nextp)=PREV_POINTER(bp);
	NEXT_POINTER(bp)=NULL;
	PREV_POINTER(bp)=NULL;
}
/*
Requires:
a pointer "bp" to the block to be inserted

Effects:
inserts the block  with address "bp" to the free list.

*/
static void insertblock(void *bp){
	/*if head is null or head is just allocated to heap-listp+DSIZE as done in mm_init()*/
	if(head==NULL||((head==heap_listp+DSIZE)&&(GET_SIZE(HDRP(bp))==4096))){
		head=(char *)bp;
		NEXT_POINTER(bp)=NULL;
		PREV_POINTER(bp)=NULL;
	}
	/*or else insert in the front*/
	else {
		PREV_POINTER(head)=bp;
		NEXT_POINTER(bp)=head;
		head=(char *)bp;
		PREV_POINTER(bp)=NULL;
 }
}
/*
 * The last lines of this file configures the behavior of the "Tab" key in
 * emacs.  Emacs has a rudimentary understanding of C syntax and style.  In
 * particular, depressing the "Tab" key once at the start of a new line will
 * insert as many tabs and/or spaces as are needed for proper indentation.
 */

/* Local Variables: */
/* mode: c */
/* c-default-style: "bsd" */
/* c-basic-offset: 8 */
/* c-continued-statement-offset: 4 */
/* indent-tabs-mode: t */
/* End: */

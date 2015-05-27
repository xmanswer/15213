/*
 * mm.c
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * Min Xu
 * andrewID: minxu
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "contracts.h"
#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

#define PRINT_CHECK_HEAP	0

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* CSAPP style macros */

#define WSIZE	4	/* Word and header/footer size  */
#define DSIZE	8	/* Doubleword size */
#define CHUNKSIZE	(1 << 6)	/* Extend heap by this amount  */

#define MAX(x, y)	((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word  */
#define PACK(size, prealloc, alloc)	((size) | (prealloc) | (alloc))
//#define PACK(size, alloc)	((size) | (alloc))

/* Read and write a word at address p  */
#define GET(p)	(*(unsigned int *) (p))
#define PUT(p, val)	(*(unsigned int *) (p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)	(GET(p) & ~0x7)
#define GET_ALLOC(p)	(GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)	((char *)(bp) - WSIZE)
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* End of CSAPP style macros */

/* Previous allocated info is stored at the second bit of header 
 * Macros for get and put alloc info in it */
#define GET_ALLOC_PREV(bp) (GET(HDRP(bp)) & 0x2)
#define HDRP_NEXT(bp) (HDRP(NEXT_BLKP(bp)))
#define PUT_ALLOC_PREV(bp) (PUT(HDRP(bp), (GET(HDRP(bp)) | 0x2)))
#define PUT_FREE_PREV(bp) (PUT(HDRP(bp), (GET(HDRP(bp)) & ~0x2)))
#define PUT_ALLOC_THIS(bp) (PUT(HDRP_NEXT(bp), (GET(HDRP_NEXT(bp)) | 0x2)))
#define PUT_FREE_THIS(bp) (PUT(HDRP_NEXT(bp), (GET(HDRP_NEXT(bp)) & ~0x2)))

/* Magic numbers for small size class segregated free lists 
 * Sizes are 8, 16, 24, 32, 40 */
#define SLIST_NUM	5	//numbers of small size class lists
#define SNUM_0	8
#define SNUM_1	16
#define SNUM_2	24
#define SNUM_3	32
#define SNUM_4	40

/* Macros for small size lists */
#define SLIST_PREV(bp)	((char *)(bp))
#define SLIST_NEXT(bp)	((char *)(bp) + WSIZE)

/* Macros for BST lists */

/* Address of pointer to the previous free block in the same size list*/
#define BST_PREV(bp)	((char *)(bp))	

/* Address of pointer to the next free block in the same size list */
#define BST_NEXT(bp)	((char *)(bp) + WSIZE)

/* Address of pointer to the left child list of this size list */
#define BST_LEFT(bp)	((char *)(bp) + 2*DSIZE)

/* Address of pointer to the right child list of this size list */
#define BST_RIGHT(bp)	((char *)(bp) + 3*DSIZE)

/* End of macros for BST lists*/


/* pointer addr will be stored as 32-bit, but need to be converted to 64-bit
 * which is the true pointer address that bp represents */
#define GET_PTR_64(bp)	((void *)((unsigned long)(bp) + 0x800000000))

/* pointer is 64-bit, but sometimes need to be cut down to 32-bit for 
 * purpose of assigning a value in list */
#define GET_32(bp)	((unsigned int)((unsigned long)(bp) - 0x800000000))


/* Global variables */
static char *heap_listp = 0;	/* Pointer to first block */
/* array of pointers to small segregated free lists */
static void *segFreeList0;	//for size <= 8
static void *segFreeList1;	//for size <= 16
static void *segFreeList2;	//for size <= 24
static void *segFreeList3;	//for size <= 32
static void *segFreeList4;	//for size <= 40
/* root of BST for larger size classes */
static void *bstRoot;


/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static inline void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);
static inline void insertNode(void *bp);
static inline void removeNode(void *bp);
static int in_heap(const void *p);
static int aligned(const void *p);
static void checkBST(void *bp);
static void checkSmallList(void *bp); 
static inline void defaultLink(void *bp);
static inline void *smallListFit(void *curPtr, void *segFreeList);
 
/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	/* Initialize list pointers */
	dbg_printf("initialize heap...\n"); 
	segFreeList0 = 0;
	segFreeList1 = 0;
	segFreeList2 = 0;   
	segFreeList3 = 0;
	segFreeList4 = 0;
	bstRoot = 0;

	/* Create the initial empty heap */
	if((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1)
		return -1;
	PUT(heap_listp, 0);	/* Alignment padding */
	PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 0, 1));	/* Prologue header */
	PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 0, 1));	/* Prologue footer */
	PUT(heap_listp + (3*WSIZE), PACK(0, 0, 1));	/* Epilogue header */
	heap_listp += (2*WSIZE);
	//PUT_ALLOC_THIS(heap_listp);	
	//mm_checkheap(PRINT_CHECK_HEAP);

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL) 
		return -1;

	return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    size_t asize;	/*  Adjusted block size */
	size_t extendsize;	/* Amount to extend heap if no fit */
	char *bp;

	
	if(heap_listp == 0)
		mm_init();
	
	/* Ignore spurious requests */
	if(size == 0)
		return NULL;

	//mm_checkheap(PRINT_CHECK_HEAP);
	
	dbg_printf("do malloc with size [%zu]...\n", size);

	/* Adjust block size to include overhead and alignment reqs */
	if(size < DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
	
	dbg_printf("input size is [%zu], actual size is [%zu]\n", size, asize);

	/* Search the free list for a fit */
	if((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	if((bp = extend_heap(extendsize/WSIZE)) != NULL) {
		place(bp, asize);
		return bp;
	}

	return NULL;
}

/*
 * free
 */
void free (void *ptr) {
	//mm_checkheap(PRINT_CHECK_HEAP);

    if(!ptr) return;
	REQUIRES(in_heap(ptr) > 0);
	REQUIRES(aligned(ptr) > 0);
	dbg_printf("free block [%p]:\n", ptr);

	size_t size = GET_SIZE(HDRP(ptr));

	if(heap_listp == 0)
		mm_init();
	
	PUT(HDRP(ptr), PACK(size, GET_ALLOC_PREV(ptr), 0));
	PUT(FTRP(ptr), PACK(size, GET_ALLOC_PREV(ptr), 0));
	//PUT_FREE_THIS(ptr);	
	insertNode(coalesce(ptr));
}

/* extend_heap - Extend heap with free block and return its block pointer */
static void *extend_heap(size_t words) {
	char *bp;
	size_t size;
	
	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	dbg_printf("extend heap size...\n");
	dbg_printf("extend words: [%zu], extend size: [%zu]\n", words, size);

	if((long)(bp = mem_sbrk(size)) == -1)
		return NULL;
	
	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, GET_ALLOC_PREV(bp), 0));	
	PUT(FTRP(bp), PACK(size, GET_ALLOC_PREV(bp), 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0, 1));	
	/* Coalesce if the previous block was free */
	return coalesce(bp);
}


/* place - Place block of asize bytes at start of free block bp
 * and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize) {
	REQUIRES(bp != 0);
	REQUIRES(in_heap(bp));
	REQUIRES(aligned(bp));
	size_t csize = GET_SIZE(HDRP(bp));
	
	dbg_printf("place a block, split if necessary...\n");
	if((csize - asize) >= (2*DSIZE)) {	//if free block is larger, split
		PUT(HDRP(bp), PACK(asize, GET_ALLOC_PREV(bp), 1));
		PUT(FTRP(bp), PACK(asize, GET_ALLOC_PREV(bp), 1));
		//PUT_ALLOC_THIS(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, GET_ALLOC_PREV(bp),  0));
		PUT(FTRP(bp), PACK(csize - asize, GET_ALLOC_PREV(bp),  0));
		//PUT_FREE_THIS(bp);
		defaultLink(bp);
		insertNode(bp);
	}
	else {	//if the same size or less than 2*DSIZE, do not split
		PUT(HDRP(bp), PACK(csize, GET_ALLOC_PREV(bp), 1));
		PUT(FTRP(bp), PACK(csize, GET_ALLOC_PREV(bp), 1));
		//PUT_ALLOC_THIS(bp);
	}
}


/* coalesce - Boundary tag coalescing. Return ptr to coalesced block */
static void *coalesce(void *bp) {
	REQUIRES(bp != 0);
	REQUIRES(in_heap(bp) > 0);
	REQUIRES(aligned(bp) > 0);
	
	dbg_printf("coalesce block [%p]...\n", bp);
	
	//size_t prev_alloc = GET_ALLOC_PREV(bp);
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if(prev_alloc && next_alloc) {	//case 1
		defaultLink(bp);
	}
	else if(prev_alloc && !next_alloc) {	//case 2
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		removeNode(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, GET_ALLOC_PREV(bp), 0));
		PUT(FTRP(bp), PACK(size, GET_ALLOC_PREV(bp), 0));
		defaultLink(bp);
	}
	else if(!prev_alloc && next_alloc) {	//case 3
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		removeNode(PREV_BLKP(bp));
		PUT(FTRP(bp), PACK(size, GET_ALLOC_PREV(PREV_BLKP(bp)), 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, GET_ALLOC_PREV(PREV_BLKP(bp)), 0));
		bp = PREV_BLKP(bp);
		defaultLink(bp);
	}
	else {	//case 4
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		removeNode(PREV_BLKP(bp));
		removeNode(NEXT_BLKP(bp));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, GET_ALLOC_PREV(PREV_BLKP(bp)), 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, GET_ALLOC_PREV(PREV_BLKP(bp)), 0));
		bp = PREV_BLKP(bp);
		defaultLink(bp);
	}

	return bp;
}

/* defaultLink - set the link pointers to 0 
 * Usually called before freeing a block */
static inline void defaultLink(void *bp) {
	REQUIRES(bp != 0);
	REQUIRES(in_heap(bp) > 0);
	REQUIRES(aligned(bp) > 0);

	if(GET_SIZE(HDRP(bp)) <= SNUM_4) {	//if size is small
		PUT(SLIST_PREV(bp), 0);
		PUT(SLIST_NEXT(bp), 0);
	}
	else {	//if size is big, default is BST style
		PUT(BST_PREV(bp), 0);
		PUT(BST_NEXT(bp), 0);
		PUT(BST_LEFT(bp), 0);
		PUT(BST_RIGHT(bp), 0);
	}
}


/* find_fit - Find a fit for a block with asize byes */
static inline void *find_fit(size_t asize) {
	
	void *curPtr;
	void *tempPtr;	//storing the tempory best fit block
	tempPtr = 0;

	if(asize > SNUM_4) {	//big size, need to find in BST lists
		/* Whole BST is empty */
		if(bstRoot == 0) {
			return NULL;
		}
		curPtr = bstRoot;
		while(GET_32(curPtr) != 0) {

			size_t sizeCurr = GET_SIZE(HDRP(curPtr));

			if(asize == sizeCurr) {	//find the correct size
				removeNode(curPtr);
				return curPtr;
			}
			/* go to the left child, find the list with the 
			 * smallest size difference, but still bigger than
			 * the target size */
			else if(asize < sizeCurr) {	//go to left child
				
				/* if tempPtr not empty and it is bigger than current
				 * size, store current pointer as the temp pointer */
				if(GET_32(tempPtr) != 0) {
					if(GET_SIZE(HDRP(tempPtr)) > sizeCurr)
						tempPtr = curPtr;
				}
				/* if tempPtr is empty, set current to be it */
				else {
					tempPtr = curPtr;
				}

				if(GET(BST_LEFT(curPtr))!= 0) {
					curPtr = GET_PTR_64(GET(BST_LEFT(curPtr)));
				}
				/* no left child left, no point to check the right child
				 * need to return tempPtr since it stores the best fit 
				 * block pointer */
				else {
					removeNode(tempPtr);
					return tempPtr;
				}
			}
			else {	//go to right child
				if(GET(BST_RIGHT(curPtr)) != 0) {
					curPtr = GET_PTR_64(GET(BST_RIGHT(curPtr)));
				}
				else {	//no exact size list, return the best fit or NULL
					if(GET_32(tempPtr) != 0) {
						removeNode(tempPtr);
						return tempPtr;
					}
					else return NULL;
				}
			}
		}
	}
	/* small size, find in small size lists */
	else if(asize <= SNUM_0) {	//size <= 8
		return smallListFit(curPtr, segFreeList0);
	}
	else if(asize <= SNUM_1) {	//size <= 16
		return smallListFit(curPtr, segFreeList1);
	}
	else if(asize <= SNUM_2) {	//size <= 24
		return smallListFit(curPtr, segFreeList2);
	}
	else if(asize <= SNUM_3) {	//size <= 32
		return smallListFit(curPtr, segFreeList3);
	}
	else if(asize <= SNUM_4) {	//size <= 40
		return smallListFit(curPtr, segFreeList4);
	}
	return NULL;
}

/* helper function for finding fit in small lists*/
static inline void *smallListFit(void *curPtr, void *segFreeList) {
	if(segFreeList == 0)
		return NULL;
	curPtr = segFreeList;	
	removeNode(curPtr);
	return curPtr;	
}

/* inserNode - depending on the size of block, decide if need to
 * insert this block into small segregated lists or BST lists */
static inline void insertNode(void *bp) {
	REQUIRES(bp != 0);
	size_t size = GET_SIZE(HDRP(bp));
	
	dbg_printf("insert node [%p]...\n", bp);

	if(size > SNUM_4) {	//large enough size, use BST
		void *curPtr;
		void *curLeft;
		void *curRight;
		void *curParent;
		curPtr = bstRoot;
		if(curPtr == 0) {	//no lists in BST yet, put bp in root
			bstRoot = bp;
			return;
		}
		while(GET_32(curPtr) != 0) { 
			if(size == GET_SIZE(HDRP(curPtr))) {	//found the correct list
				curLeft = GET_PTR_64(GET(BST_LEFT(curPtr)));
				curRight = GET_PTR_64(GET(BST_RIGHT(curPtr)));
				curParent = GET_PTR_64(GET(BST_PREV(curPtr)));

				/* insert this block as the head in BST */
				PUT(BST_NEXT(bp), GET_32(curPtr));
				PUT(BST_PREV(curPtr), GET_32(bp));


				/* configure childern */
				if(GET_32(curLeft) != 0) {
					PUT(BST_PREV(curLeft), GET_32(bp));
					PUT(BST_LEFT(bp), GET_32(curLeft));
					PUT(BST_LEFT(curPtr), 0);
				}
				if(GET_32(curRight) != 0) {				
					PUT(BST_PREV(curRight), GET_32(bp));
					PUT(BST_RIGHT(bp), GET_32(curRight));
					PUT(BST_RIGHT(curPtr), 0);
				}

				/* configure parent */
				if(GET_32(curParent)!= 0) {		
					size_t sizeParent = GET_SIZE(HDRP(curParent));
					/* set as parent's new left child head */
					if(size < sizeParent) {
						PUT(BST_LEFT(curParent), GET_32(bp));
					}
					else { //set as parent's new right child head
						PUT(BST_RIGHT(curParent), GET_32(bp));
					}
					PUT(BST_PREV(bp), GET_32(curParent));
				}
				else { //the old head is the root, set the new root
					PUT(BST_PREV(bp), 0);
					bstRoot = bp;
				}
				return;
			}
			else if(size < GET_SIZE(HDRP(curPtr))) { //go to the left child
				if(GET(BST_LEFT(curPtr)) != 0)	//found left child
					curPtr = GET_PTR_64(GET(BST_LEFT(curPtr)));
				else {	//no left child, need to insert a new list
					PUT(BST_LEFT(curPtr), GET_32(bp));
					PUT(BST_PREV(bp), GET_32(curPtr));
					return;
				}
			}
			else {	//go to the right child
				if(GET(BST_RIGHT(curPtr)) != 0)	//found right child
					curPtr = GET_PTR_64(GET(BST_RIGHT(curPtr)));
				else {	//no right child, need to insert a new list
					PUT(BST_RIGHT(curPtr), GET_32(bp));
					PUT(BST_PREV(bp), GET_32(curPtr));
					return;
				}
			}
		}
	}
	/* small size classes, use small seg lists */
	else if(size <= SNUM_0) {	//size <= 8	
		if(segFreeList0 == 0) {	//if list is empty, put bp as the head
			segFreeList0 = bp;
			return;
		}
		/* if there is a head, put bp as the new head */
		else {
			PUT(SLIST_NEXT(bp), GET_32(segFreeList0));
			PUT(SLIST_PREV(segFreeList0), GET_32(bp));
			segFreeList0 = bp;
			return;
		}
	}	
	else if(size <= SNUM_1) {	//size <= 16
		if(segFreeList1 == 0) {	//if list is empty, put bp as the head
			segFreeList1 = bp;
			return;
		}
		/* if there is a head, put bp as the new head */
		else {
			PUT(SLIST_NEXT(bp), GET_32(segFreeList1));
			PUT(SLIST_PREV(segFreeList1), GET_32(bp));
			segFreeList1 = bp;
			return;
		}
	}
	else if(size <= SNUM_2) {	//size <= 24
		if(segFreeList2 == 0) {	//if list is empty, put bp as the head
			segFreeList2 = bp;
			return;
		}
		/* if there is a head, put bp as the new head */
		else {
			PUT(SLIST_NEXT(bp), GET_32(segFreeList2));
			PUT(SLIST_PREV(segFreeList2), GET_32(bp));
			segFreeList2 = bp;
			return;
		}
	}
	else if(size <= SNUM_3) {	//size <= 32
		if(segFreeList3 == 0) {	//if list is empty, put bp as the head
			segFreeList3 = bp;
			return;
		}
		/* if there is a head, put bp as the new head */
		else {
			PUT(SLIST_NEXT(bp), GET_32(segFreeList3));
			PUT(SLIST_PREV(segFreeList3), GET_32(bp));
			segFreeList3 = bp;
			return;
		}
	}
	else if(size <= SNUM_4) {	//size <= 40
		if(segFreeList4 == 0) {	//if list is empty, put bp as the head
		segFreeList4 = bp;
		return;
		}
		/* if there is a head, put bp as the new head */
		else {
			PUT(SLIST_NEXT(bp), GET_32(segFreeList4));
			PUT(SLIST_PREV(segFreeList4), GET_32(bp));
			segFreeList4 = bp;
			return;
		}
	}
	return;
}

/* removeNode - remove a block from either small lists or BST lists */
static inline void removeNode(void *bp) {
	REQUIRES(bp != 0);
	size_t size = GET_SIZE(HDRP(bp));
	dbg_printf("remove node block [%p] from free lists...\n", bp);

	void *smallNext;
	void *smallPrev;
	
	if(size > SNUM_4) {	//big list, in BST
		REQUIRES(bstRoot != 0);
		dbg_printf("remove node from BST\n");
		void *curLeft;
		void *curRight;
		void *curNext;
		void *curParent;
		curLeft = GET_PTR_64(GET(BST_LEFT(bp)));
		curRight = GET_PTR_64(GET(BST_RIGHT(bp)));
		curNext = GET_PTR_64(GET(BST_NEXT(bp)));
		/* bp should be node, so prev of bp is
		 * its parent list */
		curParent = GET_PTR_64(GET(BST_PREV(bp)));

		/* if block is not in the head of list, simply remove */
		if(GET_32(curParent) != 0 && (GET_SIZE(HDRP(curParent)) == size)) {
			if(GET_32(curNext) != 0) {	//has next?
				PUT(BST_NEXT(curParent), GET_32(curNext));
				PUT(BST_PREV(curNext), GET_32(curParent));
			}
			else	//no next, set 0
				PUT(BST_NEXT(curParent), 0);
			return;
		}

		/* if bp is the head and has other members in the same size list
		 * move the next one to the head of the list as a node */
		if(GET_32(curNext)!= 0) {

			/* configure the new node */
			PUT(BST_LEFT(curNext), GET_32(curLeft));
			PUT(BST_RIGHT(curNext), GET_32(curRight));
			PUT(BST_PREV(curNext), GET_32(curParent));

			/* configure the left and right child */
			if(GET_32(curLeft) != 0)
				PUT(BST_PREV(curLeft), GET_32(curNext));
			if(GET_32(curRight) != 0)
				PUT(BST_PREV(curRight), GET_32(curNext));

			/* configure the parent */
			if(GET_32(curParent) != 0){
				/* parent size is bigger, put it as left child*/
				if(GET_SIZE(HDRP(curParent)) > size)
					PUT(BST_LEFT(curParent), GET_32(curNext));
				else	//parent size is smaller, put it as right child
					PUT(BST_RIGHT(curParent), GET_32(curNext));
			}
			/* no parent, target block is in root list
			 * put the next block as root */
			else {
				PUT(BST_PREV(curNext), 0);
				bstRoot = curNext;
			}
		}
		/* if bp does not have other members, need to reconfigure BST */
		else if((GET_32(curLeft) != 0) && (GET_32(curRight) != 0)) {

			/* put the left most leave of curRight in the bp position 
			 * curRight will be its right chile 
			 * curLeft will be its left child */ 
			void *newCur;
			/* curRight has no left child 
			 * curRight will be in position */
			if(GET(BST_LEFT(curRight)) == 0) {
				/* configure parent */
				PUT(BST_PREV(curRight), GET_32(curParent));
				if(GET_32(curParent) != 0) {
				/* parent size is bigger, put it as left child*/
					if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(curRight)))
						PUT(BST_LEFT(curParent), GET_32(curRight));
					else	//parent size is smaller, put it as right child
						PUT(BST_RIGHT(curParent), GET_32(curRight));
				}
				/* no parent, put right child as the root */
				else {
					PUT(BST_PREV(curRight), 0);
					bstRoot = curRight;
				}

				/* configure left child*/
				PUT(BST_PREV(curLeft), GET_32(curRight));
				PUT(BST_LEFT(curRight), GET_32(curLeft));
			}
			else {
				newCur = curRight;
				/* Go through left children of curRight
				 * find left most leaf */
				while(GET(BST_LEFT(newCur)) != 0) {
					newCur = GET_PTR_64(GET(BST_LEFT(newCur)));
				}
				/* configure newCur's parent, if newCur has right child 
				 * put it as the parent's left child*/
				void *newCurParent;
				void *newCurRight;
				newCurParent = GET_PTR_64(GET(BST_PREV(newCur)));
				newCurRight = GET_PTR_64(GET(BST_RIGHT(newCur)));
				if(GET_32(newCurRight == 0))
					PUT(BST_LEFT(newCurParent), 0);
				else {
					PUT(BST_LEFT(newCurParent), GET_32(newCurRight));
					PUT(BST_PREV(newCurRight), GET_32(newCurParent));
				}

				/* configure parent */
				PUT(BST_PREV(newCur), GET_32(curParent));
				if(GET_32(curParent) != 0) {
					/* parent size is bigger, put it as left child*/
					if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(newCur)))
						PUT(BST_LEFT(curParent), GET_32(newCur));
					else	//parent size is smaller, put it as right child
						PUT(BST_RIGHT(curParent), GET_32(newCur));
				}
				/* no parent, put newCur as the root */
				else {
					PUT(BST_PREV(newCur), 0);
					bstRoot = newCur;
				}
	
				/* configure left child */
				PUT(BST_PREV(curLeft), GET_32(newCur));
				PUT(BST_LEFT(newCur), GET_32(curLeft));	

				/* configure right child*/
				PUT(BST_PREV(curRight), GET_32(newCur));
				PUT(BST_RIGHT(newCur), GET_32(curRight));	
			}
		}
		else if((GET_32(curLeft) != 0) && (GET_32(curRight) == 0)) {

			/* if bp dose not have right child
			 * left child replaces bp and connects to parent */
			PUT(BST_PREV(curLeft), GET_32(curParent));
			if(GET_32(curParent) != 0) {
				/* parent size is bigger, put it as left child*/
				if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(curLeft)))
					PUT(BST_LEFT(curParent), GET_32(curLeft));
				else	//parent size is smaller, put it as right child
					PUT(BST_RIGHT(curParent), GET_32(curLeft));
			}
			/* no parent, put left child as the root */
			else {
				PUT(BST_PREV(curLeft), 0);
				bstRoot = curLeft;
			}	
		}
		else if((GET_32(curRight) != 0) && (GET_32(curLeft) == 0)) {	

			/* if bp dose not have left child
			 * right child replaces bp and connects to parent */
			PUT(BST_PREV(curRight), GET_32(curParent));
			if(GET_32(curParent) != 0) {
				/* parent size is bigger, put it as left child*/
				if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(curRight)))
					PUT(BST_LEFT(curParent), GET_32(curRight));
				else	//parent size is smaller, put it as right child
					PUT(BST_RIGHT(curParent), GET_32(curRight));
			}
			/* no parent, put right child as the root */
			else {
				PUT(BST_PREV(curRight), 0);
				bstRoot = curRight;
			}
		}
		else {	//no next, no childern, this block will die alone

			if(GET_32(curParent) != 0) {	//still has parent
				/* parent size is bigger, set its left child to 0*/
				if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(bp)))
					PUT(BST_LEFT(curParent), 0);
				else	//parent size is smaller, put it as right child
					PUT(BST_RIGHT(curParent), 0);

			}
			else {	//does not even have parent, set root to 0
				bstRoot = 0;
			}
		}
	}
	/* small lists */
	else if(size <= SNUM_0) {	//size <= 8
		REQUIRES(segFreeList0 != 0);
		smallNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		smallPrev = GET_PTR_64(GET(SLIST_PREV(bp)));
		
		/* if pointer is head */
		if(bp == segFreeList0) {
			/* set the head pointer to the next or NULL */
			if(GET_32(smallNext) == 0)
				segFreeList0 = 0;
			else
				segFreeList0 = smallNext;
		}
		else {	//if not the head, simply remove it
			if(GET_32(smallNext) == 0)
				PUT(SLIST_NEXT(smallPrev), 0);
			else {
				PUT(SLIST_NEXT(smallPrev), GET_32(smallNext));
				PUT(SLIST_PREV(smallNext), GET_32(smallPrev));	
			}
		}
		return;
	}
	else if(size <= SNUM_1) {	//size <= 16
		REQUIRES(segFreeList1 != 0);
		smallNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		smallPrev = GET_PTR_64(GET(SLIST_PREV(bp)));
		
		/* if pointer is head */
		if(bp == segFreeList1) {
			/* set the head pointer to the next or NULL */
			if(GET_32(smallNext) == 0)
				segFreeList1 = 0;
			else
				segFreeList1 = smallNext;
		}
		else {	//if not the head, simply remove it
			if(GET_32(smallNext) == 0)
				PUT(SLIST_NEXT(smallPrev), 0);
			else {
				PUT(SLIST_NEXT(smallPrev), GET_32(smallNext));
				PUT(SLIST_PREV(smallNext), GET_32(smallPrev));
			}
		}
		return;

	}
	else if(size <= SNUM_2) {	//size <= 24
		REQUIRES(segFreeList2 != 0);
		smallNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		smallPrev = GET_PTR_64(GET(SLIST_PREV(bp)));
		
		/* if pointer is head */
		if(bp == segFreeList2) {
			/* set the head pointer to the next or NULL */
			if(GET_32(smallNext) == 0)
				segFreeList2 = 0;
			else
				segFreeList2 = smallNext;
		}
		else {	//if not the head, simply remove it
			if(GET_32(smallNext) == 0)
				PUT(SLIST_NEXT(smallPrev), 0);
			else {
				PUT(SLIST_NEXT(smallPrev), GET_32(smallNext));
				PUT(SLIST_PREV(smallNext), GET_32(smallPrev));
			}
		}
		return;

	}
	else if(size <= SNUM_3) {	//size <= 32
		REQUIRES(segFreeList3 != 0);
		smallNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		smallPrev = GET_PTR_64(GET(SLIST_PREV(bp)));
		
		/* if pointer is head */
		if(bp == segFreeList3) {
			/* set the head pointer to the next or NULL */
			if(GET_32(smallNext) == 0)
				segFreeList3 = 0;
			else
				segFreeList3 = smallNext;
		}
		else {	//if not the head, simply remove it
			if(GET_32(smallNext) == 0)
				PUT(SLIST_NEXT(smallPrev), 0);
			else {
				PUT(SLIST_NEXT(smallPrev), GET_32(smallNext));
				PUT(SLIST_PREV(smallNext), GET_32(smallPrev));
			}
		}
		return;

	}
	else if(size <= SNUM_4) {	//size <= 40
		REQUIRES(segFreeList4 != 0);
		smallNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		smallPrev = GET_PTR_64(GET(SLIST_PREV(bp)));
		
		/* if pointer is head */
		if(bp == segFreeList4) {
			/* set the head pointer to the next or NULL */
			if(GET_32(smallNext) == 0)
				segFreeList4 = 0;
			else
				segFreeList4 = smallNext;
		}
		else {	//if not the head, simply remove it
			if(GET_32(smallNext) == 0)
				PUT(SLIST_NEXT(smallPrev), 0);
			else {
				PUT(SLIST_NEXT(smallPrev), GET_32(smallNext));
				PUT(SLIST_PREV(smallNext), GET_32(smallPrev));
			}
		}
		return;

	}
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
	
	size_t oldsize;
	void *newptr;
	
	dbg_printf("realloc block [%p] with size [%zu]...\n", oldptr, size);
	
	/* If size == 0 then this is just free, we we return NULL. */
    if(size == 0) {
		free(oldptr);
		return 0;
	}

	/* If oldptr is NULL, then this is just malloc */
	if(oldptr == NULL) {
		return malloc(size);
	}
	
	newptr = malloc(size);

	/* If realloc() fails the original block is left untouched */
	if(!newptr) {
		return 0;
	}

	/* Copy the old data. */
	oldsize = GET_SIZE(HDRP(oldptr));
	if(size < oldsize) oldsize = size;
	memcpy(newptr, oldptr, oldsize);

	/* Free the old block. */
	free(oldptr);

	return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {

	
	size_t asize = size * nmemb;
	void *curPtr;

	dbg_printf("do calloc with total size [%zu]...\n", asize);

	curPtr = malloc(asize);
	memset(curPtr, 0, asize);
	return curPtr;
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}


/* print information of a block*/ 
static void printblock(void *bp) {
	size_t hsize, halloc, fsize, falloc;
	
	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));

	if(hsize == 0) {
		printf("%p: EOL\n", bp);
		return;
	}
	
	//show general header and footer info
	printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp, hsize, \
	(halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
	
	//if free block, show link pointers info
	if(halloc == 0) {
		if(hsize <= SNUM_4) {	//small lists
			printf("SMALL LIST: PREV[0x%x] NEXT[0x%x]\n", \
			GET(SLIST_PREV(bp)), GET(SLIST_NEXT(bp)));
		}
		else {	//BST lists
			printf("BST:PREV[0x%x] NEXT[0x%x] LEFT[0x%x] RIGHT[0x%x]\n", \
			GET(BST_PREV(bp)), GET(BST_NEXT(bp)), \
			GET(BST_LEFT(bp)), GET(BST_RIGHT(bp)));
		}
	}
}

/* check block self-consistency, including consistency between
 * HDRP and FTRP, also whether block is in heap, whether it 
 * is aligned. for free blocks, will check consistency in list */
static void checkblock(void *bp) {
	REQUIRES(bp != 0);
	if((size_t)bp % 8)
		printf("Error: [%p] is not doubleword aligned\n", bp);
	if(GET(HDRP(bp)) != GET(FTRP(bp)))
		printf("Error: [%p] header does not match footer\n", bp);
	if(in_heap(bp) < 0)
		printf("Error: [%p] is not in heap\n", bp);
	if(aligned(bp) < 0)
		printf("Error: [%p] is not doubleword aligned\n", bp);
	
	/* if it is free block, check in BST or small lists*/
	if(GET_ALLOC(HDRP(bp)) == 0) {
		size_t sizebp = GET_SIZE(HDRP(bp));
		if(sizebp > SNUM_4)
			checkBST(bp);
		else
			checkSmallList(bp);
	}
}

/* check consistency of blocks in BST, including prev, next, left child
 * and right child, exit will be called once error comes out*/
static void checkBST(void *bp) {
	if(bp != 0) {	
		void *bpPrev;
		void *bpNext;
		void *bpLeft;
		void *bpRight;
		bpPrev = GET_PTR_64(GET(BST_PREV(bp)));
		bpNext = GET_PTR_64(GET(BST_NEXT(bp)));
		bpLeft = GET_PTR_64(GET(BST_LEFT(bp)));
		bpRight = GET_PTR_64(GET(BST_RIGHT(bp)));
		if(GET_32(bpPrev) != 0) {
			void *bpPrevNext;
			if(GET_SIZE(HDRP(bpPrev)) == GET_SIZE(HDRP(bp)))
				bpPrevNext = GET_PTR_64(GET(BST_NEXT(bpPrev)));
			else if(GET_SIZE(HDRP(bpPrev)) > GET_SIZE(HDRP(bp)))
				bpPrevNext = GET_PTR_64(GET(BST_LEFT(bpPrev)));
			else
				bpPrevNext = GET_PTR_64(GET(BST_RIGHT(bpPrev)));
			if(bpPrevNext == 0 || bp != bpPrevNext) {
				printf("Error: BST[%p] and [%p]'s next/children not match\n",\
				bp, bpPrev);
				printblock(bp);
				printblock(bpPrev);
				exit(0);
			}
		}
		if(GET_32(bpNext) != 0) {
			if(GET_32(bp) != GET(BST_PREV(bpNext))) {
				printf("Error: BST[%p] and [%p]'s prev does not match\n",\
				bp, bpNext);
				printblock(bp);
				printblock(bpNext);
				exit(0);
			}
			checkBST(bpNext);
		}
		if(GET_32(bpRight) != 0) {
			if(GET_32(bp) != GET(BST_PREV(bpRight))) {
				printf("Error: BST[%p] and [%p]'s parent does not match\n",\
				bp, bpRight);	
				printblock(bp);
				printblock(bpRight);
				exit(0);
			}
			checkBST(bpRight);
		}
		if(GET_32(bpLeft) != 0) {
			if(GET_32(bp) != GET(BST_PREV(bpLeft))) {
				printf("Error: BST[%p] and [%p]'s parent does not match\n",\
				bp, bpLeft);		
				printblock(bp);
				printblock(bpLeft);
				exit(0);
			}
			checkBST(bpLeft);
		}
	}
}

/* check consistency of blocks in small list, 
 * including prev, next, left child and right child, 
 * exit will be called once error comes out*/
static void checkSmallList(void *bp) {
	if(bp != 0) {
		void *bpNext;
		bpNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		if(GET_32(bpNext) != 0) {
			if(GET_32(bp) != GET(SLIST_PREV(bpNext))) {
				printf("Error: SLIST [%p] and [%p]'s prev does not match\n",\
				bp, bpNext);
				printblock(bp);
				printblock(bpNext);
				exit(0);
			}
			checkSmallList(bpNext);
		}
	}
}
/*
 * mm_checkheap
 * check prologue and epiogue, position of heap_listp
 * most importantly check correctness of each block
 * by calling checkblock
 */
void mm_checkheap(int lineno) {
	char *bp = heap_listp;
	if(lineno)
		printf("Heap (%p):\n", heap_listp);
	
	if((GET_SIZE(HDRP(heap_listp)) != DSIZE) || ! GET_ALLOC(HDRP(heap_listp)))
		printf("Bad prologue header\n");
	checkblock(heap_listp);

	for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if(lineno)
			printblock(bp);
		checkblock(bp);
	}

	if(lineno)
		printblock(bp);
	if((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
		printf("Bad epilogue header\n");
}

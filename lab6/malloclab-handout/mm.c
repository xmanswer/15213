/******************************************************************************
 ******************************************************************************
 * mm.c
 * Min Xu
 * andrewID: minxu
 ******************************************************************************
 * PROGRAM DESCRIPTION:
 * Implemented 64-bit system malloc, free, realloc and calloc using unix like 
 * system call mm_sbrk. Memory is divided into blocks and than gets allocated.
 * Each block has their header and footer, which store identical information
 * about their size and allocation info. Prologue and Epilogue blocks are 
 * applied in order to align the data instead of the header of each block to 
 * double word. Freed memory blocks will be insert into free lists for future
 * allocation use. Program will maintain the free lists, including finding,
 * inserting and removing a block into or from the free lists.  
 * This program used segregated lists methods for storing free blocks. 
 ******************************************************************************
 * BASIC FUNCTIONS DESCRIPTION:
 * extend_heap - extend the heap based on the requested size. Extended blocks
 * will be free blocks and the pointer to the start of the newly extended block
 * will be returned
 * place - mark the requested block as allocated and split the block into
 * allocated block and free block if necessary, the newly generated free block
 * with smaller size will be re-inserted back to free lists
 * coalesce - for a newly freed block, connect it with previous or next block 
 * if they are also free blocks
 * find_fit - find a free block and reture the block pointer using the best fit
 * stratege, the block size will not be smaller than the requested size
 * insertNode - insert a block as a node in BST lists or small list based on 
 * its block size
 * removeNode - remove a block from BST lists or small list based on its size
 ******************************************************************************
 * BINARY SEARCH TREE FREE LISTS:
 * For big size blocks (requested size > 8 bytes), they will be stored in
 * binary search tree (BST) lists. Each node in the tree is the head of a
 * list with certain size. The head of another list, with bigger block size,
 * will be stored as the right child node of current node in BST, oposite is
 * true for smaller size list, which will be left child. For each block in the
 * BST lists, the high level data structure is formed by following sections:
 * |HEADER|PREVIOUS ADDR|NEXT ADDR|LEFT ADDR|RIGHT ADDR|FOOTER|
 * For node in BST, the previous block will be its parent, while for other
 * blocks who are not the head of their list, the previous block is just its 
 * previous block in that list. Although the address is 64-bit, we only need
 * 32-bit to store the whole info for each address, since the heap start at
 * (0x800000000) and we know its total size is only 32-bit. So each section 
 * only takes 4 bytes except for data section. So the minimum size for a BST 
 * block is 24 bytes. To maintain the BST in a relatively balanced condition,
 * removing node from a BST tends to choose the leaf of the tree. 
 * Sepecifically, the left most leaf of the removed node's right child will 
 * have the highest priority to replace the position of the removed node,
 * then the right child, then the left child.
 ******************************************************************************
 * SMALL LIST:
 * Small list only holds for one size. Following is the data structure:
 * |HEADER|PREVIOUS ADDR|NEXT ADDR|FOOTER|
 * Including the linking pointer info, the small list has a fixed size of 16
 * bytes. Maintaining the small list will help reduce the fregmentations and
 * improve utilization.
 ******************************************************************************
 *****************************************************************************/
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

#define PRINT_CHECK_HEAP	0 //for printing block info in mm_checkheap

/* macro define for mm_checkheap function */
#ifdef DEBUG
#define CHECKHEAP(verbose) printf("%s\n", __func__); mm_checkheap(verbose);
#else
#define CHECKHEAP(verbose)
#endif

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
#define CHUNKSIZE	((1 << 6))	/* Extend heap by this amount  */

#define MAX(x, y)	((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word  */
//#define PACK(size, prealloc, alloc)	((size) | (prealloc) | (alloc))
#define PACK(size, alloc)	((size) | (alloc))

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

#define SSIZE	16 //maximum size for storing in small list

/* Macros for small size list */
/* Address of pointer to the previous free block in the small list */
#define SLIST_PREV(bp)	((char *)(bp))
/* Address of pointer to the next free block in the small list */
#define SLIST_NEXT(bp)	((char *)(bp) + WSIZE)

/* Macros for BST lists */
/* Address of pointer to the previous free block in the same size list */
#define BST_PREV(bp)	((char *)(bp))	
/* Address of pointer to the next free block in the same size list */
#define BST_NEXT(bp)	((char *)(bp) + WSIZE)
/* Address of pointer to the left child list of this size list */
#define BST_LEFT(bp)	((char *)(bp) + 2*WSIZE)
/* Address of pointer to the right child list of this size list */
#define BST_RIGHT(bp)	((char *)(bp) + 3*WSIZE)

/* pointer addr will be stored as 32-bit, but need to be converted to 64-bit
 * which is the true pointer address that bp represents */
#define GET_PTR_64(bp)	((void *)((unsigned long)(bp) + 0x800000000))

/* pointer is 64-bit, but sometimes need to be cut down to 32-bit for 
 * purpose of assigning a value in list */
#define GET_32(bp)	((unsigned int)((unsigned long)(bp) - 0x800000000))

/* Global variables */
static char *heap_listp = 0;	/* Pointer to first block */

static void *bstRoot; /* root pointer of BST list */
static void *segFreeList; /* head pointer of small list */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static inline void *find_fit(size_t asize);
static inline void *find_fit_bst(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);
static inline void insertNode(void *bp);
static inline void removeNode(void *bp);
static inline void insertNodeBST(void *bp);
static inline void removeNodeBST(void *bp);
static int in_heap(const void *p);
static int aligned(const void *p);
static void checkBST(void *bp);
static void checkSmallList(void *bp);
static inline void defaultLink(void *bp);


 
/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	
	dbg_printf("initialize heap...\n"); 
	
	/* Initialize list pointers */
  segFreeList = 0;
	bstRoot = 0;

	/* Create the initial empty heap */
	if((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1)
		return -1;
	PUT(heap_listp, 0);	/* Alignment padding */
	PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));	/* Prologue header */
	PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));	/* Prologue footer */
	PUT(heap_listp + (3*WSIZE), PACK(0, 1));	/* Epilogue header */
	heap_listp += (2*WSIZE);

	CHECKHEAP(PRINT_CHECK_HEAP);

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL) 
		return -1;

	return 0;
}

/*
 * malloc - allocate given size in heap
 */
void *malloc (size_t size) {
	size_t asize;	/*  Adjusted block size */
	size_t extendsize;	/* Amount to extend heap if no fit */
	char *bp;

	if(heap_listp == 0)
		mm_init();
	
	/* Ignore spurious requests */
	if(size <= 0)
		return NULL;

	CHECKHEAP(PRINT_CHECK_HEAP);
	
	dbg_printf("do malloc with size [%zu]...\n", size);

	/* Adjust block size to include overhead and alignment reqs */
	if(size < DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + DSIZE - 1) / DSIZE) + DSIZE;
	
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
 * free - free given pointer block from the heap
 */
void free (void *ptr) {

	CHECKHEAP(PRINT_CHECK_HEAP);

	if(!ptr) return;
	REQUIRES(in_heap(ptr) > 0);
	REQUIRES(aligned(ptr) > 0);
	dbg_printf("free block [%p]:\n", ptr);

	size_t size = GET_SIZE(HDRP(ptr));

	if(heap_listp == 0)
		mm_init();
	
	/* Configure header and footer */
	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	insertNode(coalesce(ptr)); //reinsert coalesced block
}

/*
 * realloc - re-allocate a given pointer for another given size
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
 * calloc - allocate a block with given size with 0 filled
 */
void *calloc (size_t nmemb, size_t size) {

	size_t bytes = nmemb * size;
	void *newptr;

	dbg_printf("do calloc with total size [%zu]...\n", bytes);

	newptr = malloc(bytes);
	memset(newptr, 0, bytes);
	return newptr;
}

/* extend_heap - Extend heap with free block and return its block pointer */
static void *extend_heap(size_t words) {
	char *bp;
	size_t size;
	
	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

	dbg_printf("heap extended: words: [%zu], size: [%zu]\n", words, size);

	if((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));	
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));	
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
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
		/* After header/footer configuration default block's linked
		 * pointers info and insert into free list*/
		defaultLink(bp);
		insertNode(bp);
	}
	else {	//if the same size or less than 2*DSIZE, do not split
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}

/* coalesce - Boundary tag coalescing. Return ptr to coalesced block */
static void *coalesce(void *bp) {
	REQUIRES(bp != 0);
	REQUIRES(in_heap(bp) > 0);
	REQUIRES(aligned(bp) > 0);
	
	dbg_printf("coalesce block [%p]...\n", bp);
	
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if(prev_alloc && next_alloc) {	//case 1, both are allocated
		defaultLink(bp);
	}
	else if(prev_alloc && !next_alloc) {	//case 2, next block is free
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		removeNode(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		defaultLink(bp);
	}
	else if(!prev_alloc && next_alloc) {	//case 3, previous block is free
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		removeNode(PREV_BLKP(bp));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
		defaultLink(bp);
	}
	else {	//case 4, both are free
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		removeNode(PREV_BLKP(bp));
		removeNode(NEXT_BLKP(bp));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
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

	if(GET_SIZE(HDRP(bp)) <= SSIZE) {	//if size is small
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
	if(asize > SSIZE) //size larger than 16, find in BST list
		return find_fit_bst(asize);
	else { //size smaller than 16, find in small list
		void *curPtr; //simply remove and return the head
		if(segFreeList == 0)
			return NULL;
		curPtr = segFreeList;	
		removeNode(curPtr);
		return curPtr;
	}
	return NULL;
}

/* find_fit_bst - Find a fit for given size in BST list 
 * Use best fit to find the block with slightly bigger size
 * but smallest difference than other blocks */
static inline void *find_fit_bst(size_t asize) {
	
	void *curPtr;
	void *tempPtr;	//storing the tempory best fit block
	tempPtr = 0;

	/* Whole BST is empty */
	if(bstRoot == 0) {
		return NULL;
	}	
	curPtr = bstRoot;
	while(GET_32(curPtr) != 0) {
		if(asize == GET_SIZE(HDRP(curPtr))) {	//find the correct size
			removeNodeBST(curPtr);
			return curPtr;
		}
		else if(asize < GET_SIZE(HDRP(curPtr))) {	//curr size bigger
			/* if tempPtr not empty and bigger than current size
			 * store current pointer as the temp pointer */
			if(GET_32(tempPtr) != 0) {
				if(GET_SIZE(HDRP(tempPtr)) > GET_SIZE(HDRP(curPtr)))
					tempPtr = curPtr;
			}
			/* if tempPtr is empty, set current to be it */
			else {
				tempPtr = curPtr;
			}
			if(GET(BST_LEFT(curPtr))!= 0) { //go to left child
				curPtr = GET_PTR_64(GET(BST_LEFT(curPtr)));
			}
			/* no left child, just return tempPtr 
			 * it stores the best fit block pointer */
			else {
				removeNodeBST(tempPtr);
				return tempPtr;
			}
		}
		else { //curr size smaller
			if(GET(BST_RIGHT(curPtr)) != 0) { //go to right child
				curPtr = GET_PTR_64(GET(BST_RIGHT(curPtr)));
			}
			else {	//no exact size list, return the best fit or NULL
				if(GET_32(tempPtr) != 0) {
					removeNodeBST(tempPtr);
					return tempPtr;
				}
				else return NULL;
			}
		}
	}
 		return NULL;
}

/* inserNode - depending on the size of block, decide if need to
 * insert this block into small list or BST lists */
static inline void insertNode(void *bp) {
	REQUIRES(bp != 0);
	size_t size = GET_SIZE(HDRP(bp));
	if(size > SSIZE) //size bigger than 16, insert to BST
		insertNodeBST(bp);
	else { //size smaller than 16, insert to small list
		if(segFreeList == 0) {	//if list is empty, put bp as the head
			segFreeList = bp;
		}
		else { // if there is a head, put bp as the new head
			PUT(SLIST_NEXT(bp), GET_32(segFreeList));
			PUT(SLIST_PREV(segFreeList), GET_32(bp));
			segFreeList = bp;
		}
	}
}

/* inserNodeBST - insert a node into BST lists, search for the correct
 * size first, if found, put it as the next of the found block, if not,
 * put as either left or right child */
static inline void insertNodeBST(void *bp) {
	REQUIRES(bp != 0);
	size_t size = GET_SIZE(HDRP(bp));
	
	dbg_printf("insert node [%p]...\n", bp);

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
				/* set as parent's new left child head */
				if(size < GET_SIZE(HDRP(curParent))) {
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

/* removeNode - remove a block from either small list or BST lists */
static inline void removeNode(void *bp) {
	REQUIRES(bp != 0);
	size_t size = GET_SIZE(HDRP(bp));
	if(size > SSIZE) //size bigger than 16, remove from BST lists
		removeNodeBST(bp);
	else { //size smaller than 16, remove from small list
		void *smallNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		void *smallPrev = GET_PTR_64(GET(SLIST_PREV(bp)));
		REQUIRES(segFreeList0 != 0);
		if(bp == segFreeList) {		//if bp is head, put its next as head
			if(GET_32(smallNext) == 0)
				segFreeList = 0;
			else
				segFreeList = smallNext;
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


/* removeNodeBST - remove a node from BST lists, statege for choosing new
 * head is that, the block with just slightly bigger size has the highest
 * priority, this is usually the left most leaf of its right child */
static inline void removeNodeBST(void *bp) {
	REQUIRES(bp != 0);
	size_t size = GET_SIZE(HDRP(bp));
	dbg_printf("remove node block [%p] from free lists...\n", bp);
	
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
		if(GET_32(curNext) != 0) {	//has next, connect next to parent
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
	/* if bp does not have other members, need to reconfigure BST
	 * put the left most leaf of curRight in the bp position 
	 * curRight as its right child, curLeft as its left child */
	else if((GET_32(curLeft) != 0) && (GET_32(curRight) != 0)) {
		void *newCur;
		/* curRight has no left child, so it will replace bp */
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
		else { //find the left most leaf or curRight
			newCur = curRight;
			while(GET(BST_LEFT(newCur)) != 0) {
				newCur = GET_PTR_64(GET(BST_LEFT(newCur)));
			}
			/* configure newCur's parent, if newCur has right child 
			 * put it as the parent's left child */
			void *newCurParent;
			void *newCurRight;
			newCurParent = GET_PTR_64(GET(BST_PREV(newCur)));
			newCurRight = GET_PTR_64(GET(BST_RIGHT(newCur)));
			if(GET_32(newCurRight) == 0)
				PUT(BST_LEFT(newCurParent), 0);
			else {
				PUT(BST_LEFT(newCurParent), GET_32(newCurRight));
				PUT(BST_PREV(newCurRight), GET_32(newCurParent));
			}

			/* configure parent's children based on size*/
			PUT(BST_PREV(newCur), GET_32(curParent));
			if(GET_32(curParent) != 0) {
				if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(newCur)))
					PUT(BST_LEFT(curParent), GET_32(newCur));
				else	
					PUT(BST_RIGHT(curParent), GET_32(newCur));
			}
			else { //no parent, put newCur as the root
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
	/* bp has no right child, left child will replace bp */
	else if((GET_32(curLeft) != 0) && (GET_32(curRight) == 0)) {
		PUT(BST_PREV(curLeft), GET_32(curParent));
		if(GET_32(curParent) != 0) { //configure parent
			if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(curLeft)))
				PUT(BST_LEFT(curParent), GET_32(curLeft));
			else	
				PUT(BST_RIGHT(curParent), GET_32(curLeft));
		}
		else { //no parent, put left child as the root
			PUT(BST_PREV(curLeft), 0);
			bstRoot = curLeft;
		}	
	}
	/* bp has no left child, right child will replace bp */
	else if((GET_32(curRight) != 0) && (GET_32(curLeft) == 0)) {	
		PUT(BST_PREV(curRight), GET_32(curParent));
		if(GET_32(curParent) != 0) { //configure parent
			if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(curRight)))
				PUT(BST_LEFT(curParent), GET_32(curRight));
			else
				PUT(BST_RIGHT(curParent), GET_32(curRight));
		}
		else { //no parent, put right child as the root
			PUT(BST_PREV(curRight), 0);
			bstRoot = curRight;
		}
	}
	else {	//no next, no childern, this block will die alone
		if(GET_32(curParent) != 0) {	//still has parent
			if(GET_SIZE(HDRP(curParent)) > GET_SIZE(HDRP(bp)))
				PUT(BST_LEFT(curParent), 0);
			else
				PUT(BST_RIGHT(curParent), 0);
		}
		else {	//does not even have parent, set root to 0
			bstRoot = 0;
		}
	}
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

/* Followed are all help functions for mm_checkheap */

/* print information of a block */ 
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
		if(hsize <= SSIZE) {	//small list
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
		if(sizebp > SSIZE)
			checkBST(bp);
		else
			checkSmallList(bp);
	}
}

/* check consistency of blocks in BST, including prev, next, left child
 * and right child, exit will be called once error comes out */
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
			//determine if bpPrev is the parent or in the same size list
			if(GET_SIZE(HDRP(bpPrev)) == GET_SIZE(HDRP(bp)))
				bpPrevNext = GET_PTR_64(GET(BST_NEXT(bpPrev)));
			else if(GET_SIZE(HDRP(bpPrev)) > GET_SIZE(HDRP(bp)))
				bpPrevNext = GET_PTR_64(GET(BST_LEFT(bpPrev)));
			else
				bpPrevNext = GET_PTR_64(GET(BST_RIGHT(bpPrev)));
			//check if bpPrev's next is bp
			if(bpPrevNext == 0 || bp != bpPrevNext) {
				printf("Error: BST[%p] and [%p]'s next/children not match\n",\
				bp, bpPrev);
				printblock(bp);
				printblock(bpPrev);
				exit(0);
			}
		}
		if(GET_32(bpNext) != 0) {
			//check if bpNext's previous is bp
			if(GET_32(bp) != GET(BST_PREV(bpNext))) {
				printf("Error: BST[%p] and [%p]'s prev does not match\n",\
				bp, bpNext);
				printblock(bp);
				printblock(bpNext);
				exit(0);
			}
			//check if their sizes are equal
			if(GET_SIZE(HDRP(bpNext)) != GET_SIZE(HDRP(bp))) {
				printf("Error: BST[%p] and [%p]'s next size does not match\n",\
				bp, bpNext);
				printblock(bp);
				printblock(bpNext);
				exit(0);
			}
			checkBST(bpNext); //recursively check next block
		}
		if(GET_32(bpRight) != 0) {
			//check if bp is bpRight's parent
			if(GET_32(bp) != GET(BST_PREV(bpRight))) {
				printf("Error: BST[%p] and [%p]'s parent does not match\n",\
				bp, bpRight);	
				printblock(bp);
				printblock(bpRight);
				exit(0);
			}
			//check if bpRight's size is bigger than bp
			if(GET_SIZE(HDRP(bpRight)) <= GET_SIZE(HDRP(bp))) {
				printf("Error: BST[%p]'s size bigger than [%p]'s right\n",\
				bp, bpRight);
				printblock(bp);
				printblock(bpRight);
				exit(0);
			}
			checkBST(bpRight); //recursively check right child
		}
		if(GET_32(bpLeft) != 0) {
			//check if bpLeft's parent is bp
			if(GET_32(bp) != GET(BST_PREV(bpLeft))) {
				printf("Error: BST[%p] and [%p]'s parent does not match\n",\
				bp, bpLeft);		
				printblock(bp);
				printblock(bpLeft);
				exit(0);
			}
			//check if bpLeft's size is smaller than bp
			if(GET_SIZE(HDRP(bpLeft)) >= GET_SIZE(HDRP(bp))) {
				printf("Error: BST[%p]'s size smaller than [%p]'s left\n",\
				bp, bpLeft);
				printblock(bp);
				printblock(bpLeft);
				exit(0);
			}
			checkBST(bpLeft); //recursively check left child
		}
	}
}

/* check consistency of blocks in small list */
static void checkSmallList(void *bp) {
	if(bp != 0) {
		if(GET(SLIST_PREV(bp)) == 0) { //bp has no prev
			//check if bp is the head, if not, something is wrong
			if(bp != segFreeList) {
				printf("Error: SLIST [%p] doesn't match the head [%p]\n",\
				bp, segFreeList);
				printblock(bp);
				exit(0);
			}
		}
		void *bpNext;
		bpNext = GET_PTR_64(GET(SLIST_NEXT(bp)));
		if(GET_32(bpNext) != 0) {
			if(GET_32(bp) != GET(SLIST_PREV(bpNext))) {
				printf("Error: SLIST [%p] doesn't match [%p]'s prev \n",\
				bp, bpNext);
				printblock(bp);
				printblock(bpNext);
				exit(0);
			}
			if(GET_SIZE(HDRP(bp)) != GET_SIZE(HDRP(bpNext))) {
				printf("Error: SLIST [%p] and [%p] size don't match\n",\
				bp, bpNext);
				printblock(bp);
				printblock(bpNext);
				exit(0);			
			}
			checkSmallList(bpNext); //recursively check next block
		}
	}
}


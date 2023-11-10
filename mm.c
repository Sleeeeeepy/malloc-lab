/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team 3",
    /* First member's full name */
    "Jeongmin Jo",
    /* First member's email address */
    "dede7030@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE     4 /* Word and header/footer size (bytes) */
#define DSIZE     8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((unsigned char *)(bp) - WSIZE)
#define FTRP(bp) ((unsigned char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((unsigned char *)(bp) + GET_SIZE(((unsigned char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((unsigned char *)(bp) - GET_SIZE(((unsigned char *)(bp) - DSIZE)))

typedef enum  {
    ZERO_BLK = 0,
    FREE_BLK = 0,
    ALLOC_BLK = 1
} block_status_t;

/* Declarations */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);
static void place(void *bp, size_t asize);
static void set_next_fit_ptr(void *bp);

/* Heap list */
static void *heap_listp = NULL;

/* Memory allocation strategy */
static void *(*malloc_strategy)(size_t asize);

/* 
 * Latest block pointer for next_fit. 
 * DO NOT WRITE VALUES HERE DIRECTLY.
 * USE set_next_fit_ptr(void *) INSTEAD
 */
static unsigned char *next_fit_ptr;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return - 1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, ALLOC_BLK));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, ALLOC_BLK));
    PUT(heap_listp + (3 * WSIZE), PACK(0, FREE_BLK));
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }

    /* Select memory allocation strategy */
    malloc_strategy = next_fit;
    set_next_fit_ptr(heap_listp);
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;   /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    unsigned char *bp;

    /* Ignore spurious reqeusts */
    if (size == 0) {
        return NULL;
    }

    /* Adjusted block size to include overhead and alignment reqs. */
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
	return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, FREE_BLK));
    PUT(FTRP(ptr), PACK(size, FREE_BLK));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t original_size = GET_SIZE(HDRP(ptr));
    size += 2 * WSIZE;
    if (original_size >= size) {
        return ptr;
    }

    if (original_size < size) {
        size_t total_size = original_size + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        block_status_t next = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));

        if (next == FREE_BLK && total_size >= size) {
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, ZERO_BLK));
            PUT(FTRP(ptr), PACK(0, ZERO_BLK));
            PUT(HDRP(ptr), PACK(total_size, ALLOC_BLK));
            PUT(FTRP(ptr), PACK(total_size, ALLOC_BLK));

            return ptr;
        }
    }

    // Allocate a new block and copy the data
    void *new_p = mm_malloc(size);
    if (new_p == NULL) {
        return NULL;
    }
    memcpy(new_p, ptr, size);
    mm_free(ptr);
    return new_p;
}

static void *extend_heap(size_t words)
{
    unsigned char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, FREE_BLK));           /* Free block header */
    PUT(FTRP(bp), PACK(size, FREE_BLK));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, ALLOC_BLK));  /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }
    
    if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, FREE_BLK));
        PUT(FTRP(bp), PACK(size, FREE_BLK));
        set_next_fit_ptr(bp);
        return bp;
    }

    if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, FREE_BLK));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, FREE_BLK));
        bp = PREV_BLKP(bp);
        set_next_fit_ptr(bp);
        return bp;
    }

    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, FREE_BLK));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, FREE_BLK));
    bp = PREV_BLKP(bp);
    set_next_fit_ptr(bp);
    return bp;
}

static void *find_fit(size_t asize)
{
    return malloc_strategy(asize);
}

static void *first_fit(size_t asize) {
    for (unsigned char *cursor = heap_listp; GET_SIZE(HDRP(cursor)) > 0; cursor = NEXT_BLKP(cursor)) {
        if (!GET_ALLOC(HDRP(cursor)) && (asize <= GET_SIZE(HDRP(cursor)))) {
            return cursor;
        }
    }
      return NULL;
}

static void *next_fit(size_t asize) {
    for (unsigned char *cursor = next_fit_ptr; GET_SIZE(HDRP(cursor)) > 0; cursor = NEXT_BLKP(cursor)) {
        if (!GET_ALLOC(HDRP(cursor)) && (asize <= GET_SIZE(HDRP(cursor)))) {
            set_next_fit_ptr(cursor);
            return cursor;
        }
    }

    // if a fit field does not exist, then do like 'first fit'.
    for (unsigned char *cursor = heap_listp; cursor < next_fit_ptr; cursor = NEXT_BLKP(cursor)) {
        if (!GET_ALLOC(HDRP(cursor)) && (asize <= GET_SIZE(HDRP(cursor)))) {
            set_next_fit_ptr(cursor);
            return cursor;
        }
    }

    set_next_fit_ptr(heap_listp);
    return NULL;
}


static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, ALLOC_BLK));
        PUT(FTRP(bp), PACK(asize, ALLOC_BLK));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, FREE_BLK));
        PUT(FTRP(bp), PACK(csize-asize, FREE_BLK));
        return;
    }

    PUT(HDRP(bp), PACK(csize, ALLOC_BLK));
    PUT(FTRP(bp), PACK(csize, ALLOC_BLK));
    set_next_fit_ptr(bp);
}

static void set_next_fit_ptr(void* bp) {
    next_fit_ptr = bp;
}
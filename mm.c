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
    "tawan",
    /* First member's full name */
    "Tawan Chaeyklinthes",
    /* First member's email address */
    "chaeyklinthestawan@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define ALIGNMENT 8
#define INIT_BLOCKSIZE  (1 << 12)

// p is the pointer to the header or footer
#define PACK(size, flag) (size | flag)
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, value) (*(unsigned int *)(p) = value)

#define GET_SIZE(p) (GET(p) & ~7)
#define GET_ALLOC(p) (GET(p) & 1)
#define NEXT_HDR(p) (GET_SIZE(p)

// bp points to the start of a payload
// cast it so that we can do pointer arithmatic
// NOTE: If size of block is changed, change the header before footer.
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + (GET_SIZE(HDRP(bp))) - DSIZE )
#define PREV_BLK(bp) ((char *)(bp) - (GET_SIZE(bp - DSIZE)))
#define NEXT_BLK(bp) ((char *)(bp) + (GET_SIZE(bp - WSIZE)))
 
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

void * prolp; // prologue pointer
int mm_init(void);
void *expand_heap(size_t words);
void *coalesce(void *p);
void *mm_malloc(size_t size);
void mm_free(void *p);
void *mm_realloc(void *ptr, size_t size);
void mm_check();

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    prolp = mem_sbrk(WSIZE * 4);
    if(prolp == (void *)-1){
        return -1;
    }

    //create padding , prologue, epilogue
    PUT(prolp, 0);
    PUT(prolp+ WSIZE,PACK(8,1));
    PUT(prolp + WSIZE*2, PACK(8,1));
    PUT(prolp + WSIZE*3, PACK(0,1));
    prolp += WSIZE;
    // mm_check();
    
    //create a free block in the empty heap
    void *bp = expand_heap(INIT_BLOCKSIZE / WSIZE);
    return 0;
}

/*
    Expand the heap by "words" words 
    and return a pointer to the payload of the new area.
*/
void *expand_heap(size_t words){
    // Calculate bytes of 8 bytes-aligned words
    size_t size = words % 2 ? (words+1)*WSIZE : words*WSIZE;
    void *bp = mem_sbrk(size);

    //Set new header, footer and epilogue
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(FTRP(bp)+WSIZE, PACK(0,1));
    // mm_check();
    return coalesce(bp);
}

/*
    Using the block pointer ,join the free blocks on left and right and return a 
    block pointer of the join block.
*/
void *coalesce(void *bp){
    int prev_alloc = GET_ALLOC(HDRP(PREV_BLK(bp)));
    int next_alloc = GET_ALLOC(HDRP(NEXT_BLK(bp)));


    if(prev_alloc && next_alloc){
        return bp;
    }

    if(!prev_alloc && next_alloc){
        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLK(bp))); 
        PUT(HDRP(PREV_BLK(bp)), PACK(newsize, 0));
        PUT(FTRP(PREV_BLK(bp)), PACK(newsize,0));
        return PREV_BLK(bp);
    }

    if(prev_alloc && !next_alloc){
        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLK(bp)));
        PUT(HDRP(bp), PACK(newsize, 0));
        PUT(FTRP(bp), PACK(newsize,0));
        return bp;
    }

    if(!prev_alloc && !next_alloc){
        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLK(bp))) + GET_SIZE(HDRP(NEXT_BLK(bp)));
        PUT(HDRP(PREV_BLK(bp)), PACK(newsize,0));
        PUT(FTRP(PREV_BLK(bp)), PACK(newsize,0));
        return PREV_BLK(bp);
    }
}


/* 
 * mm_malloc - Allocate a block of "size+DSIZE" bytes by first finding best-fit
 * block. If the block fits perfectly in the best-fit block, we just assigned it.
 * Else we split the block into allocated and free blocks. If no such block exist,
 * we expands the heap size by "size+DSIZE" bytes. Returns block pointer
 * 
 * NOTE: The block must be 8-aligned.
 */
void *mm_malloc(size_t size)

{
    // mm_check();
    void *p = prolp + DSIZE;// First header
    void *bfp = NULL; // Header of best-fit block *(not payload)
    size_t asize = ALIGN(size) + DSIZE; //Aligned size + hrd + ftr


    //Find best-fit block by looping till epilogue
    while(GET(p) != PACK(0,1)){
        if(GET_ALLOC(p) == 0 && GET_SIZE(p) >= asize){ // Check if block free and big enough
            if(bfp == NULL || GET_SIZE(p) < GET_SIZE(bfp)){ // Check if block smaller than current best
                bfp = p;
            }
        }
        p = NEXT_BLK(p + WSIZE) - WSIZE;// Point to next header
    }

    /*
     * If good free block exist we assigned header and footer .
     * If block doesn't fit perfectly we split the block into allocated
     * and free blocks, both with hdr and ftr.
    */

    if (bfp != NULL){ // Check if free block exist
        size_t remaining = GET_SIZE(bfp) - asize;
        PUT(bfp, PACK(asize,1));
        PUT(FTRP(bfp + WSIZE), PACK(asize,1));

        if(remaining > 0){ // Check if there any free space
            // Create hdr and ftr for free block
            PUT(HDRP(NEXT_BLK(bfp + WSIZE)), PACK(remaining, 0));
            PUT(FTRP(NEXT_BLK(bfp + WSIZE)), PACK(remaining,0));
        }
        return bfp + WSIZE;
    }

    /* 
     * If no free block, expands the heap then create hdr and ftr.
    */
    void *bp = expand_heap(asize/WSIZE);
    PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)),1));
    PUT(FTRP(bp), PACK(GET_SIZE(FTRP(bp)),1));
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
    PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * Checker 
*/
void mm_check(){
    printf("====================================================================\n");
    void *p = prolp;
    printf("Prologue : %p, Size: %ut, Flag: %d\n",p,GET_SIZE(p),GET_ALLOC(p));
    p += GET_SIZE(p);
    while(GET(p) != PACK(0,1)){
        printf("Header : %p, Size: %ut, Flag: %d\n",p,GET_SIZE(p),GET_ALLOC(p));
        p += GET_SIZE(p);
    }
    printf("Epilogue : %p, Size: %ut, Flag: %d\n",p,GET_SIZE(p),GET_ALLOC(p));
}
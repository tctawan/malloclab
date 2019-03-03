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

// p is the pointer to the header/footer/pred/succ
#define PACK(size, flag) (size | flag)
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, value) (*(unsigned int *)(p) = value)
#define GET_ADDR(p) (*(char **)(p))
#define PUT_ADDR(p,addr) (*(char **)(p) = addr)

#define GET_SIZE(p) (GET(p) & ~7)
#define GET_ALLOC(p) (GET(p) & 1)

// bp points to the start of a payload
// cast it so that we can do pointer arithmatic
// NOTE: If size of block is changed, change the header before footer.
#define HDRP(bp) ((char *)(bp) - WSIZE*3)
#define FTRP(bp) ((char *)(bp) + (GET_SIZE(HDRP(bp))) - DSIZE*2 )

// Go to next or previous payload
#define PREV_BLK(bp) ((char *)HDRP(bp) - (GET_SIZE(HDRP(bp) - WSIZE) - 3*WSIZE))
#define NEXT_BLK(bp) ((char *)FTRP(bp) + 4*WSIZE)

// Pred and Succ of the block
#define PRED(bp) (bp - DSIZE)
#define SUCC(bp) (bp - WSIZE)

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

int mm_init(void);
void *expand_heap(size_t words);
void *coalesce(void *bp);
void *link_to_root(void *bp);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);
void mm_check();
void splice(void* bp);
void *find(size_t asize);

void *root; // root pointer
void *epilogue;// epilogue pointer

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    root = mem_sbrk(DSIZE * 4);
    if(root == (void *)-1){
        return -1;
    }

    //create padding , prologue, epilogue
    PUT(root, 0);
    PUT(root+ WSIZE,PACK(WSIZE*6,1));
    PUT(root + WSIZE*2, NULL);
    PUT(root + WSIZE*3, NULL);
    PUT(root + WSIZE*4, 1);// dummy payload
    PUT(root + WSIZE*5, 1);
    PUT(root + WSIZE*6, PACK(WSIZE*6,1));
    PUT(root + WSIZE*7, PACK(0,1));
    epilogue = root+WSIZE*7;
    root += WSIZE*4;
    //create a free block in the empty heap
    void *bp = expand_heap(INIT_BLOCKSIZE / WSIZE);
    // printf("+++++++++++++++++MM_INIT EXECUTED+++++++++++++++\n");
    // mm_check();
    return 0;
}

/*
    Expand the heap by "words" words 
    and return a pointer to the payload of the new area.
*/
void *expand_heap(size_t words){
    // Calculate bytes of 8 bytes-aligned words
    size_t size = words % 2 ? (words+1)*WSIZE : words*WSIZE;
    void *bp = mem_sbrk(size) + DSIZE; // offset it to a block pointer

    //Set new header, footer, epilogue, pred, succ
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT_ADDR(HDRP(bp)+WSIZE, NULL); // set pred
    PUT_ADDR(HDRP(bp)+DSIZE, NULL); // set succ
    PUT(FTRP(bp)+WSIZE, PACK(0,1));
    bp =coalesce(bp);
    epilogue = FTRP(bp) + WSIZE;

    // printf("+++++++++++++++++HEAP EXPANDED+++++++++++++++\n");
    // mm_check();
    return bp;
}

/*
    Using the block pointer, first coalesce the free blocks then 
    redirecting the pointer. Returns the pointer to the payload of 
    the coalesced block.
*/
void *coalesce(void *bp){
    int prev_alloc = GET_ALLOC(HDRP(PREV_BLK(bp)));
    int next_alloc = GET_ALLOC(HDRP(NEXT_BLK(bp)));
    // printf("prev %d, next %d\n", prev_alloc, next_alloc);


    if(prev_alloc && next_alloc){
        return link_to_root(bp);
    }

    if(!prev_alloc && next_alloc){// left block is free

        // Splice the previous block
        splice(PREV_BLK(bp));

        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLK(bp))); 
        
        // Change footer and header(size & alloc)
        PUT(HDRP(PREV_BLK(bp)), PACK(newsize, 0));
        PUT(FTRP(PREV_BLK(bp)), PACK(newsize,0));

        return link_to_root(PREV_BLK(bp));
    }

    if(prev_alloc && !next_alloc){ // right block is free
        //Splice the next block
        splice(NEXT_BLK(bp));

        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLK(bp)));

        // Change footer and header(size & alloc)
        PUT(HDRP(bp), PACK(newsize, 0));
        PUT(FTRP(bp), PACK(newsize,0));

    
        return link_to_root(bp);
    }

    if(!prev_alloc && !next_alloc){ // both left and right blocks are free

        //Splice both previous block and next block
        splice(PREV_BLK(bp));
        splice(NEXT_BLK(bp));

        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLK(bp))) + GET_SIZE(HDRP(NEXT_BLK(bp)));

        // Change footer and header(size & alloc)
        PUT(HDRP(PREV_BLK(bp)), PACK(newsize,0));
        PUT(FTRP(PREV_BLK(bp)), PACK(newsize,0));

        return link_to_root(PREV_BLK(bp));
    }
}

/**
 * Using the block pointer, insert the joined block to the linked list
*/
void *link_to_root(void *bp){

    PUT_ADDR(SUCC(bp),GET_ADDR(SUCC(root))); // connect succ of current block to the succ of root.
    PUT_ADDR(PRED(bp), root);// connect pred of current block to root.
    if(GET_ADDR(SUCC(bp)) != NULL){
        PUT_ADDR(PRED(GET_ADDR(SUCC(bp))), bp); // if exist, connect the pred of current block succ to current block.
    }
    PUT_ADDR(SUCC(root), bp);// connect succ of root to current block

    return bp;
}

/**
 * Using the block pointer, splice the block.
*/
void splice(void *bp){
    if(GET_ADDR(PRED(bp)) != NULL){
        // Point pred to succ
        PUT_ADDR(SUCC(GET_ADDR(PRED(bp))),GET_ADDR(SUCC(bp)));
    }
    if(GET_ADDR(SUCC(bp)) != NULL){
        //Point succ to pred
        PUT_ADDR(PRED(GET_ADDR(SUCC(bp))), GET_ADDR(PRED(bp)));
    }
}


/* 
 * mm_malloc - Allocate a block of "size+DSIZE*2" bytes by first finding best-fit
 * block. Looking though the linked-list of free blocks,if the block fits perfectly
 * in the best-fit block, we just assigned it.
 * Else we split the block into allocated and free blocks and redirect pointers. 
 * If no such block exist,we expands the heap size by "size+DSIZE" bytes.
 * Returns block pointer.
 * 
 * NOTE: The block must be 8-aligned.
 */
void *mm_malloc(size_t size)

{
    size_t asize = ALIGN(size) + DSIZE*2; //Aligned size + hrd + ftr + pred + succ
    void *bfp = find(asize); // Find best-fit block and assigned the pointer to payload to bfp.
    /**
     * Check if big enough block exist. If it doesnt, expand the heap.
    */


    if (bfp == NULL){
        // Get size from footer of last block. If last block is not free, set last_block_size to 0.
        size_t last_block_size = GET_ALLOC(epilogue-WSIZE) == 0 ? GET_SIZE(epilogue - WSIZE): 0;
        bfp = expand_heap((asize-last_block_size)/WSIZE);
    }

    /*
     * Once we have a big enough block, we assigned header and footer .
     * If block doesn't fit perfectly we split the block into allocated
     * and free blocks, both with hdr and ftr.
    */

    size_t remaining = GET_SIZE(HDRP(bfp)) - asize;

    /**
     * This makes sure that the free space is still a valid block
     * (contains header,payload,pred,succ,footer).
     */
    if(remaining < 24 && remaining > 0){
            void *bp = expand_heap((24-remaining)/WSIZE);
            remaining = GET_SIZE(HDRP(bp)) - asize;
    }

    // Rewrite the header and footer with new block size.
    PUT(HDRP(bfp), PACK(asize,1));
    PUT(FTRP(bfp), PACK(asize,1));

    if(remaining != 0){ // Check if there any free space
        // Create hdr and ftr for free block
        PUT(HDRP(NEXT_BLK(bfp)), PACK(remaining, 0));
        PUT(FTRP(NEXT_BLK(bfp)), PACK(remaining,0));
        // Re-link pred and succ to the remaining free block.
        PUT(PRED(NEXT_BLK(bfp)), GET_ADDR(PRED(bfp)));
        PUT(SUCC(NEXT_BLK(bfp)), GET_ADDR(SUCC(bfp)));

        if(GET_ADDR(PRED(bfp)) != NULL){
            // If the predecessor is not null, point succ of the predecessor to free block
            PUT(SUCC(GET_ADDR(PRED(bfp))), NEXT_BLK(bfp));
        }

        if(GET_ADDR(SUCC(bfp)) != NULL){
            // If the successor is not null, point pred of the successor to free block
            PUT(PRED(GET_ADDR(SUCC(bfp))), NEXT_BLK(bfp));
        }
    }else{
        splice(bfp);
    }
    PUT_ADDR(SUCC(bfp), NULL);
    PUT_ADDR(PRED(bfp), NULL);

    return bfp;
}

/**
 * Find the best-fit block of size "asize" bytes. Returns
 * a pointer to the start of payload.
*/

void *find(size_t asize){
    void *bp = GET_ADDR(SUCC(root));//Frist free payload 
    void *bfp = NULL; // Payload of best-fit block

    //Find best-fit block by looping till epilogue
    while(bp != NULL){
        if(GET_SIZE(HDRP(bp)) >= asize){ // Check big enough
            if(bfp == NULL || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(bfp))){ // Check if the block is smaller than current best
                bfp = bp;
            }
        }
        bp = GET_ADDR(SUCC(bp));// Point to next free payload
    }

    return bfp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // printf("----------------FREE %p------------\n", bp);
    PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
    PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
    coalesce(bp);
    // mm_check();
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
    void *p = HDRP(root);
    void *bp = root;
    printf("Root Header : %p, Root Payload: %p ,Size: %ut, Flag: %d, Pred: %p, Succ %p\n",p,bp,GET_SIZE(HDRP(bp)),
    GET_ALLOC(HDRP(bp)), GET_ADDR(PRED(bp)), GET_ADDR(SUCC(bp)));
    p = FTRP(bp) + WSIZE;
    while(GET(p) != PACK(0,1)){
        bp = p + 3*WSIZE;
        printf("Header : %p, Payload : %p, Size: %ut, Flag: %d, Pred: %p, Succ %p\n",p,bp,GET_SIZE(HDRP(bp)),
    GET_ALLOC(HDRP(bp)), GET_ADDR(PRED(bp)), GET_ADDR(SUCC(bp)));
        p = FTRP(bp) + WSIZE;
    }
    printf("Epilogue : %p, Size: %ut, Flag: %d\n",p,GET_SIZE(p),GET_ALLOC(p));
    printf("====================================================================\n");
}
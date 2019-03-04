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
#include <math.h>

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

#define GET_START(bucket_num) (bucket[bucket_num])

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
int get_bucket(size_t size);
void put_bucket(int bucket_num, void *bp);

void *prologue; // root pointer
void *epilogue;// epilogue pointer
void *bucket[64]; // buckets that keep bp of free block of that size class.

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    prologue = mem_sbrk(DSIZE * 4);
    if(prologue == (void *)-1){
        return -1;
    }

    memset(bucket, NULL, sizeof bucket);

    //create padding , prologue, epilogue
    PUT(prologue, 0);
    PUT(prologue+ WSIZE,PACK(WSIZE*6,1));
    PUT(prologue + WSIZE*2, NULL);
    PUT(prologue + WSIZE*3, NULL);
    PUT(prologue + WSIZE*4, 1);// dummy payload
    PUT(prologue + WSIZE*5, 1);
    PUT(prologue + WSIZE*6, PACK(WSIZE*6,1));
    PUT(prologue + WSIZE*7, PACK(0,1));
    epilogue = prologue+WSIZE*7;
    prologue += WSIZE*4;

    //create a free block in the empty heap
    void *bp = expand_heap(INIT_BLOCKSIZE / WSIZE);
    // printf("+++++++++++++++++MM_INIT EXECUTED+++++++++++++++\n");
    // mm_check();
    return 0;
}

/*
    Expand the heap by "words" words and return a pointer to the payload of the new area.
    Request for more memmory -> set new free block -> coalesce with free block at the end ->
    move the epilogue.
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

/**
 * Takes size of block and returns int of bucker number.
 * Get the bucket number for size of block.
*/
int get_bucket(size_t size){
    size--;
    for( int i=0; i<6; i++){
        size |= size >> (1 << i);
    }
    size++;
    return (log(size)/log(2));
}

/**
 * Takes bucket_num and bp. Put the free block into the
 * bucket of its class size by adding to the front of the linked list.
 * 
*/
void put_bucket(int bucket_num, void *bp){
    // printf("bucket_num: %d, bp: %p\n", bucket_num,bp);
    void *start = GET_START(bucket_num);
    if(start != NULL){
        PUT_ADDR(PRED(start), bp);
    }
    bucket[bucket_num] = bp;
    PUT_ADDR(PRED(bp), NULL);
    PUT_ADDR(SUCC(bp), start);

}

/*
    Take in a block pointer returns a block pointer to the coalesced block.
    Check next and prev block allocate flag -> splice the block (if needed) ->
    linked the joined block to the root.
*/
void *coalesce(void *bp){
    int prev_alloc = GET_ALLOC(HDRP(PREV_BLK(bp)));
    int next_alloc = GET_ALLOC(HDRP(NEXT_BLK(bp)));
    // printf("prev %d, next %d\n", prev_alloc, next_alloc);


    if(prev_alloc && next_alloc){
        put_bucket(get_bucket(GET_SIZE(HDRP(bp))),bp);
        return bp;
    }

    if(!prev_alloc && next_alloc){// left block is free

        // Splice the previous block
        splice(PREV_BLK(bp));

        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLK(bp))); 
        // printf("newsize : %ut\n", newsize);
        
        // Change footer and header(size & alloc)
        PUT(HDRP(PREV_BLK(bp)), PACK(newsize, 0));
        PUT(FTRP(PREV_BLK(bp)), PACK(newsize,0));

        put_bucket(get_bucket(newsize), PREV_BLK(bp));

        return PREV_BLK(bp);
    }

    if(prev_alloc && !next_alloc){ // right block is free
        //Splice the next block
        splice(NEXT_BLK(bp));

        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLK(bp)));

        // Change footer and header(size & alloc)
        PUT(HDRP(bp), PACK(newsize, 0));
        PUT(FTRP(bp), PACK(newsize,0));

        put_bucket(get_bucket(newsize), bp);

        return bp;
    }

    if(!prev_alloc && !next_alloc){ // both left and right blocks are free

        //Splice both previous block and next block
        splice(PREV_BLK(bp));
        splice(NEXT_BLK(bp));

        size_t newsize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLK(bp))) + GET_SIZE(HDRP(NEXT_BLK(bp)));

        // Change footer and header(size & alloc)
        PUT(HDRP(PREV_BLK(bp)), PACK(newsize,0));
        PUT(FTRP(PREV_BLK(bp)), PACK(newsize,0));

        put_bucket(get_bucket(newsize), PREV_BLK(bp));

        return PREV_BLK(bp);
    }
}

/**
 * Takes a block pointer of the "unlinked" free block and returns the same block pointer.
 * Connect SUCC to root's SUCC and PRED to root -> Connect root's SUCC to bp -> 
 * Connect succ's PRED to bp (if succ exist).
*/
// void *link_to_root(void *bp){

//     PUT_ADDR(SUCC(bp),GET_ADDR(SUCC(root))); // connect succ of current block to the succ of root.
//     PUT_ADDR(PRED(bp), root);// connect pred of current block to root.
//     PUT_ADDR(SUCC(root), bp);// connect succ of root to current block

//     if(GET_ADDR(SUCC(bp)) != NULL){
//         // if exist, connect the pred of current block succ to current block.
//         PUT_ADDR(PRED(GET_ADDR(SUCC(bp))), bp); 
//     }

//     return bp;
// }

/**
 * Takes a block pointer to block in the linked list and splice it (free the block from linked list).
*/
void splice(void *bp){
    int bucket_number  = get_bucket(GET_SIZE(HDRP(bp)));
    if(GET_START(bucket_number) != bp){ // is not root

        if(GET_ADDR(PRED(bp)) != NULL){
            // Point pred to succ
            PUT_ADDR(SUCC(GET_ADDR(PRED(bp))),GET_ADDR(SUCC(bp)));
        }
        if(GET_ADDR(SUCC(bp)) != NULL){
            //Point succ to pred
            PUT_ADDR(PRED(GET_ADDR(SUCC(bp))), GET_ADDR(PRED(bp)));
        }
    }else{
        bucket[bucket_number] = GET_ADDR(SUCC(bp));

        if(GET_ADDR(SUCC(bp)) != NULL){
            PUT_ADDR(PRED(GET_ADDR(SUCC(bp))), NULL);
        }

    }
    PUT_ADDR(SUCC(bp), NULL);
    PUT_ADDR(PRED(bp), NULL);
}


/* 
 * mm_malloc - Allocate a block of "size+DSIZE*2" bytes by first finding best-fit
 * block. If there is no available block, expands the heap. Returns the block pointer of the
 * allocated block.
 * 
 * NOTE: The block must be 8-aligned.
 */
void *mm_malloc(size_t size)

{

    size_t asize = ALIGN(size) + DSIZE*2; // Aligned-size + hrd + ftr + pred + succ
    // printf("size to malloc : %ut\n", asize);
    void *bfp = find(asize); // Find best-fit block and assigned the pointer to payload to bfp.
    
    /**
     * If there is no available blcok, expand the heap so that the last block is 
     * big enough for asize.
    */
    if (bfp == NULL){
        // Get size from footer of last block. If last block is not free, set last_block_size to 0.
        size_t last_block_size = GET_ALLOC(epilogue-WSIZE) == 0 ? GET_SIZE(epilogue - WSIZE): 0;

        bfp = expand_heap((asize-last_block_size)/WSIZE);
    }

    /**
     * Once we found a big enough block, we calculate for the remainder .
     * If there is no remainder, the block fits perfectly and we splice it.
     * 
     * If there is remainder,we make sure that the free block is a valid block (24 bytes). 
     * If the free space is not a valid block, we allocate it .
     * 
     * We then update the header and footer. 
     * 
     * After the update, if there is a free block, we update the header and footer of free block.
     * Then connect the SUCC & PRED of free block to SUCC & PRED of bfp.
     * Then connect the succ's PRED to free block ( if succ exist ) and
     * connect the pred's SUCC to free block (if pred exist).
     */

    size_t remaining = GET_SIZE(HDRP(bfp)) - asize;
    // printf("bfp_size: %ut, asize: %ut, remaining : %ut\n",GET_SIZE(HDRP(bfp)) ,asize,remaining);

    /**
     * If the remainder is not a valid block, just allocate the remainder as well.
     * (contains header,payload,pred,succ,footer).
     */
    if(remaining < 24 && remaining > 0){
            asize = GET_SIZE(HDRP(bfp)); 
            remaining = 0;
    }

    splice(bfp);
    // printf("************************ AFTER SPLICE *************\n");
    // mm_check();

    // Rewrite the header and footer with new block size.
    PUT(HDRP(bfp), PACK(asize,1));
    PUT(FTRP(bfp), PACK(asize,1));

    if(remaining != 0){

        // Create hdr and ftr for free block
        PUT(HDRP(NEXT_BLK(bfp)), PACK(remaining, 0));
        PUT(FTRP(NEXT_BLK(bfp)), PACK(remaining,0));
        
        // Re-link pred and succ to the remaining free block.
        put_bucket(get_bucket(remaining), NEXT_BLK(bfp));
    }

    // printf("+++++++++++++++++MM_MALLOC %ut+++++++++++++++\n", asize);
    // mm_check();

    return bfp;
}

/**
 * Find the best-fit block of size "asize" bytes. Returns
 * a pointer to the start of payload.
*/
void *find(size_t asize){
    void *bfp = NULL; // Payload of best-fit block
    // printf("bucket to looked at : %d\n",get_bucket(asize));
    //Find best-fit block by looping till no more free block
    for (int bucket_num = get_bucket(asize); bucket_num < 64; bucket_num++){

        void *bp = GET_START(bucket_num);

        while(bp != NULL){
            if(GET_SIZE(HDRP(bp)) >= asize){ // Check big enough

                // Check if the block is smaller than current best
                if(bfp == NULL || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(bfp))){
                    bfp = bp;
                }
            }
            bp = GET_ADDR(SUCC(bp));// Point to next free payload
        }
        if(bfp != NULL){
            break;
        }
    }

    return bfp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // mm_check();    
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

    if(ptr == NULL){
        return mm_malloc(size);
    }
    if(size == 0){
        mm_free(ptr);
        return ptr;
    }


    size_t copySize = size < GET_SIZE(HDRP(ptr)) ? size : GET_SIZE(HDRP(ptr));
    void *newptr=  mm_malloc(size);
    // printf("SIZE: %ut, COPYSIZE: %ut, newptr: %p\n", size, copySize, newptr);
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    // printf("After--------------------------\n");
    // mm_check();
    return newptr;
}

/*
 * Checker 
*/
void print_heap(){
    printf("====================================================================\n");
    void *p = HDRP(prologue);
    void *bp = prologue;
    printf("Prologue : %p, Prologue Payload: %p ,Size: %ut, Flag: %d, Pred: %p, Succ %p\n",p,bp,GET_SIZE(HDRP(bp)),
    GET_ALLOC(HDRP(bp)), GET_ADDR(PRED(bp)), GET_ADDR(SUCC(bp)));
    p = FTRP(bp) + WSIZE;
    while(GET(p) != PACK(0,1)){
        bp = p + 3*WSIZE;
        printf("Bucket : %d, Payload : %p, Size: %ut, Flag: %d, Pred: %p, Succ %p\n",get_bucket(GET_SIZE(HDRP(bp))),bp,GET_SIZE(HDRP(bp)),
    GET_ALLOC(HDRP(bp)), GET_ADDR(PRED(bp)), GET_ADDR(SUCC(bp)));
        p = FTRP(bp) + WSIZE;
    }
    printf("Epilogue : %p, Size: %ut, Flag: %d\n",p,GET_SIZE(p),GET_ALLOC(p));

    for (int b=0; b<64; b++){
        if(GET_START(b) != NULL){
            printf("Bucket NO: %d, Start: %p\n", b,GET_START(b));
        }
    }

    printf("====================================================================\n");
}

void mm_check(){

    //----------------------------BLOCK LEVEL------------------------------

    
}



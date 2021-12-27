/*
  Karol Ochman-Milarski 316057
  Oświadczam, że jestem jedynym autorem tego rozwiązania.
*/

// Poniższe rozwiązanie to implicit list with boundary tags.

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.
 * When you hand in, remove the #define DEBUG line. */
// #define DEBUG
#ifdef DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", __func__, __VA_ARGS__)
#define msg(...) printf(__VA_ARGS__)
#else
#define debug(fmt, ...)
#define msg(...)
#endif

#define __unused __attribute__((unused))

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* !DRIVER */

// unsigned
typedef uint32_t word_t; /* Heap is bascially an array of 4-byte words. */

typedef enum {
  FREE = 0,     /* Block is free */
  USED = 1,     /* Block is used */
  PREVFREE = 2, /* Previous block is free (optimized boundary tags) */
} bt_flags;

typedef struct {
  word_t header;
  /*
   * We don't know what the size of the payload will be, so we will
   * declare it as a zero-length array.  This allow us to obtain a
   * pointer to the start of the payload.
   */
  uint8_t payload[];
} block_t;


// static word_t *heap_start; /* Address of the first block */
// static word_t *heap_end;   /* Address past last byte of last block */
// static word_t *last;       /* Points at last block */
static word_t *begin_bl;   /* Address of the begining sentinel block */
static word_t *close_bl; /* Address of the closing sentinel block */

/* --=[ boundary tag handling ]=-------------------------------------------- */

#define HDR_SZ_WORDS 1
static const size_t HEADER_SIZE = HDR_SZ_WORDS * sizeof(word_t);

static inline word_t bt_size(word_t *bt) {
  return *bt & ~(USED | PREVFREE);
}

static inline int bt_used(word_t *bt) {
  return *bt & USED;
}

static inline int bt_free(word_t *bt) {
  return !(*bt & USED);
}

/* Given boundary tag address calculate it's buddy address. */
// Given header calculate footer address. */ 
static inline word_t *bt_footer(word_t *bt) {
  return (void *)bt + bt_size(bt) - HEADER_SIZE;
}

static inline void bt_make_footer(word_t *bt) {
  memcpy((void*)bt_footer(bt), (void*)bt, HEADER_SIZE);
  // *((word_t*)bt_footer(bt)) = *bt;
}

/* Given payload pointer returns an address of boundary tag. */
static inline word_t *bt_fromptr(void *ptr) {
  return (word_t *)ptr - HDR_SZ_WORDS;
}

/* Creates boundary tag(s) for given block. */
static inline void bt_make(word_t *bt, size_t size, bt_flags flags) {
  if (bt)
    *bt = size | flags;
}

/* Previous block free flag handling for optimized boundary tags. */
static inline bt_flags bt_get_prevfree(word_t *bt) {
  return *bt & PREVFREE;
}

static inline void bt_clr_prevfree(word_t *bt) {
  if (bt)
    *bt &= ~PREVFREE;
}

static inline void bt_set_prevfree(word_t *bt) {
  if (bt)
    *bt |= PREVFREE;
}

static inline void bt_set_used(word_t *bt) {
  if (bt)
    *bt |= USED;
}

static inline void bt_clr_used(word_t *bt) {
  if (bt)
    *bt &= ~USED;
}

/* Returns address of payload. */
static inline void *bt_payload(word_t *bt) {
  return bt + HDR_SZ_WORDS;
}

/* Returns address of next block or NULL. */
static inline word_t *bt_next(word_t *bt) {
  return  (void *)bt + bt_size(bt);
}

/* Returns address of previous block. Assumes bt has prev_free set. */
static inline word_t *bt_prev(word_t *bt) {
  return (void*)bt - bt_size(bt_fromptr(bt));
}

/* --=[ miscellanous procedures ]=------------------------------------------ */

static size_t round_up(size_t size) {
  return (size + ALIGNMENT - 1) & -ALIGNMENT;
}

/* Calculates block size incl. header, footer & payload,
 * and aligns it to block boundary (ALIGNMENT). */
static inline size_t blksz(size_t user_size) {
  return round_up( user_size + HEADER_SIZE );
}

static void *morecore(size_t size) {
  void *ptr = mem_sbrk(size);
  if (ptr == (void *)-1)
    return NULL;
  return ptr;
}

/* --=[ mm_init ]=---------------------------------------------------------- */

/* If there's not enough space on the heap for block starting at bl of bl_size bytes,
  then expand the heap to accomodate.
*/
static bool expand(word_t * bl, size_t bl_size) {

  if ( (unsigned long)bl + (unsigned long)bl_size > (unsigned long)mem_heap_hi() + 1 ) {
    unsigned long offset = (unsigned long)bl + (unsigned long)bl_size - 1 - (unsigned long)mem_heap_hi();
    void * ptr = morecore(offset);
    if (!ptr)
      return false;
    else {
      return true;
    }
  }
  return true;
}

static bool append_close_bl_after_bl(word_t * last_bl) {
  word_t * nxt = bt_next(last_bl);
  if (expand(nxt, blksz(0))) {
    bt_make(nxt, blksz(0), USED);
    close_bl = nxt;
    return true;
  } else {
    return false;
  }
}


/* Creates sentinel blocks begin_bl and close_bl. */
int mm_init(void) {

  word_t * start_bl = (void*)mem_heap_lo() + ALIGNMENT - HEADER_SIZE;
  if (!expand(start_bl , blksz(0)))
    return -1;
  
  bt_make(start_bl, blksz(0), USED);

  if (!append_close_bl_after_bl(start_bl))
    return -1;

  begin_bl = start_bl; 


  return 0;
}

/* --=[ coalesce ]=----------------------------------------------------------- */

word_t * coalesce(word_t * bl) {
  // coalescing with left neighbour
  if (bt_get_prevfree(bl)) {
    word_t *prev = bt_prev(bl);
    bt_make(prev, bt_size(prev) + bt_size(bl), FREE);
    bl = prev;
  }

  // coalescing with right neighbour
  if (bt_free( bt_next(bl) )) {
    bt_make(bl, bt_size(bl) + bt_size(bt_next(bl)), FREE);
  }

  // update prev_free flags
  bt_set_prevfree(bt_next(bl));
  return bl;
}


/* --=[ malloc ]=----------------------------------------------------------- */

#if 0
/* First fit startegy. */
static word_t *find_fit(size_t reqsz) {
}
#else
/* Best fit startegy. */
static word_t *find_fit(size_t reqsz) {
  
  size_t size = blksz(reqsz);
  for (word_t* i=begin_bl; i<close_bl ; i=bt_next(i)) {
    if (bt_free(i) && (bt_size(i) >= size))
      return i;
  }
  return close_bl;
}
#endif

/* 
  Splits block of size bl_size in two blocks by creating headers - one starting at bl of size offset, and second starting at bl+offset. 
  First block is allocated and second is a new free block.  
  Doesn't make footer. prev_free is bt_get_prevfree() of the resulting first block
*/
void split_block(word_t* bl, size_t offset, size_t bl_size, bt_flags prev_free) {
  word_t *new = (void*)bl + offset;
  bt_make(new, bl_size - offset, FREE);
  new = coalesce(new);
  bt_make_footer(new);
  bt_make(bl, offset, USED | prev_free);
}

void *malloc(size_t user_size) {

  size_t size = blksz(user_size);
  word_t * found = find_fit(user_size);

  if (found == close_bl) {
    // no free block found, append new one at the end
    bt_flags prev_free = bt_get_prevfree(close_bl);
    if (!expand(close_bl, size))
      return NULL;
    bt_make(close_bl, size, USED | prev_free);

    word_t * user_ptr = bt_payload(close_bl);
    if (!append_close_bl_after_bl(close_bl))
      return NULL;
    return user_ptr;

  } else {
    // found a fitting free block
    if ((bt_size(found) - size) >= ALIGNMENT) {
      // block is split
      split_block(found, size, bt_size(found), bt_get_prevfree(found));
      return bt_payload(found);
    } else {
      // not split
      bt_set_used(found);
      bt_clr_prevfree(bt_next(found));
      return bt_payload(found);
    }

  }
}

/* --=[ free ]=------------------------------------------------------------- */

// Frees the given block and does coalescing. Doesn't make footer.
word_t * free_but_no_footer(void *user_ptr) {
  word_t *bl = bt_fromptr(user_ptr);
  bt_clr_used(bl);
  
  bl = coalesce(bl);
  return bl;
}

void free(void *user_ptr) {
  if (user_ptr) {
    word_t * hdr = free_but_no_footer(user_ptr);
    bt_make_footer(hdr);
  }
}

/* --=[ realloc ]=---------------------------------------------------------- */

static inline size_t min(size_t a, size_t b) {
  return a <= b ? a : b;
}

void *realloc(void *old_ptr, size_t user_size) {

  if (! old_ptr)
    return malloc(user_size);

  if (user_size == 0) {
    free(old_ptr);
    return NULL;
  }

  // Case: Allocate at the same address.
  word_t *hdr = bt_fromptr(old_ptr);
  if (bt_size(hdr) >= blksz(user_size)) {
    if (bt_size(hdr) - blksz(user_size) >= ALIGNMENT) {
      split_block(hdr, blksz(user_size), bt_size(hdr), bt_get_prevfree(hdr));  
      return old_ptr;
    } else
      return old_ptr;
  }

  // remember old size to know what to copy after free.
  size_t copy_size = min( bt_size(hdr) - HEADER_SIZE, user_size );

  // Case: 
  //    - there is free block on the left and its size suffices
  //    - and the two blocks would get split if chosen by malloc resulting in payload corruption
  if (bt_get_prevfree(hdr) && (bt_size(bt_prev(hdr)) + bt_size(hdr) >= blksz(user_size)) 
    && (bt_size(bt_prev(hdr)) + bt_size(hdr) - blksz(user_size) >= ALIGNMENT)) {
    // move contents first and next set up headers
    word_t * prev = bt_prev(hdr); 
    size_t resulting_size = bt_size(prev) + bt_size(hdr);
    memmove(bt_payload(prev), old_ptr, copy_size);  
    split_block(prev, blksz(user_size), resulting_size, 0);
    return bt_payload(prev);
  }

  // Case: free first and reallocate.

  // free the block but don't create footer. The payload won't be touched.
  word_t * freed_ptr = free_but_no_footer(old_ptr);
  word_t *new = malloc(user_size);
  if (!new)
    return NULL;
    
  // old_ptr and new can have overlapping areas, use memmove
  memmove(new, old_ptr, copy_size);

  // Now either freed_ptr is a free block or new = freed_ptr
  // If the former then make footer
  if (bt_free(freed_ptr))
    bt_make_footer(freed_ptr);

  return new;
}

/* --=[ calloc ]=----------------------------------------------------------- */

void *calloc(size_t nmemb, size_t size) {
  size_t bytes = nmemb * size;
  void *new_ptr = malloc(bytes);
  if (new_ptr)
    memset(new_ptr, 0, bytes);
  return new_ptr;
}

/* --=[ mm_checkheap ]=----------------------------------------------------- */

void mm_checkheap(int verbose) {
    for (word_t* i=begin_bl; i<close_bl ; i=bt_next(i)) {
      // if (verbose) {
      //   if (bt_free(i))
      //     printf("f%d ", bt_size(i));
      //   else 
      //     printf("%d ", bt_size(i));
      // }

      if (bt_size(i) <= 0) {
        exit(EXIT_FAILURE);
      }
      if (bt_next(i) <= i) {
        exit(EXIT_FAILURE);
      }
      if (bt_free(close_bl)) {
        exit(EXIT_FAILURE);
      }
      if (bt_get_prevfree(i) && !bt_free(bt_prev(i))) {
        exit(EXIT_FAILURE);
      }
      if (bt_free(i) && bt_free(bt_next(i))) {
        exit(EXIT_FAILURE);
      }
      if (bt_size(begin_bl) != ALIGNMENT) {
        exit(EXIT_FAILURE);
      }
      if (bt_size(close_bl) != blksz(0)) {
        exit(EXIT_FAILURE);
      }
      if (bt_size(i) % ALIGNMENT != 0) {
        if (verbose)
          printf("rozmiar NIEPODZIELNY\n");
        exit(EXIT_FAILURE);
      }
      if (bt_free(i) && bt_get_prevfree(i)) {
        if (verbose)
          printf("FREE AND PREVFREE\n");
        exit(EXIT_FAILURE);
      }
      if ((unsigned long)i % ALIGNMENT != 12) {
        if (verbose)
          printf("NOT ALIGNED\n");
        exit(EXIT_FAILURE);
      }
      if ((void*)close_bl + bt_size(close_bl) > mem_heap_hi() + 1) {
        if (verbose) {
          printf("close_bl outside of heap\n");
        }
        exit(EXIT_FAILURE);
      }
      if (bt_free(i) && (*i != *bt_footer(i))) {
        if (verbose) {
          printf("FOOTER DOESN'T MATCH HEADER.\n");
        }
        exit(EXIT_FAILURE);
      }

  }
  // if (verbose)
  //   printf("|\n");
}

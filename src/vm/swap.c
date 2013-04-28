#include <debug.h>
#include "stdio.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#define PAGESECTORSIZE 8

// Function Prototype
static size_t findslot(struct swap_t * st); 

struct swap_t *swap_init()
{
   struct swap_t * st = (struct swap_t *) malloc(sizeof(struct swap_t));
   if(st == NULL)
      return NULL;

   struct block * swapdisk = block_get_role (BLOCK_SWAP);
   if(swapdisk == NULL)
      return NULL;

    st->swapblock = swapdisk;
    st->size = block_size(swapdisk) / PAGESECTORSIZE;
	st->bitmap = bitmap_create(st->size);
	st->inuse = 0;
	lock_init(&st->lock);
   	return st;
}

bool swap_read(size_t slot, struct swap_t * st, void* readptr)
{
   uint32_t i;
   for (i = 0; i < PAGESECTORSIZE; ++i)
   {
      block_read (st->swapblock, slot * PAGESECTORSIZE + i, readptr);
	  readptr = (void*) (((uint8_t*) readptr) + BLOCK_SECTOR_SIZE);
   }
   return true;
}

void swap_write(struct swap_t * st, void *writeptr, size_t * slot)
{
   if(*slot == BITMAP_ERROR)
	   *slot = findslot(st);

   //printf("Thread: %d SwapSlot1: %d", thread_current()->tid, *slot);

   // No More Stack Space
   if(*slot == BITMAP_ERROR)
      return;

   uint32_t i;
   for (i = 0; i < PAGESECTORSIZE; ++i)
   {
      block_write (st->swapblock, *slot * PAGESECTORSIZE + i, writeptr);
	  writeptr = (void*) (((uint8_t*) writeptr) + BLOCK_SECTOR_SIZE);
   }
}

void swap_delete(struct swap_t * st, size_t slot)
{
   lock_acquire (&st->lock);
   //printf("Delete - Thread: %d SwapSlot: %d\n", thread_current()->tid, slot);
   bitmap_set_multiple (st->bitmap, slot, 1, false);
   --st->inuse;
   lock_release(&st->lock);
}

size_t findslot(struct swap_t * st) 
{
   lock_acquire (&st->lock);
   size_t slot;
   slot = bitmap_scan_and_flip(st->bitmap, 0, 1, false);
   lock_release(&st->lock);
   return slot;
}

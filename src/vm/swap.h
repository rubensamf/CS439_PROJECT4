#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <debug.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "lib/kernel/bitmap.h"

struct swap_t
{
	struct block * swapblock; /* Swap Disk Block Struct */
    struct bitmap * bitmap;   /* Track Swap Slots */
	int size;                 /* Number of Swap Slots */
	uint32_t inuse;           /* Number of Used Swap Slots */
	struct lock lock;         /* Lock for Swap Table */
};

struct swap_t * swap_init(void);
bool swap_read(size_t slot, struct swap_t * st, void* readptr);
void swap_write(struct swap_t * st, void *writeptr, size_t * slot);
void swap_delete(struct swap_t * st, size_t slot);

#endif

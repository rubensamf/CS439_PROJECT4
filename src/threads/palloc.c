#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/init.h"
#include "userprog/pagedir.h"
#include "vm/spage.h"
#include "vm/swap.h"

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

// Page Eviction Algorithm
void* frame_eviction(struct frame** framelist, enum palloc_flags flags, void* upage);
void write_dirty_page(struct frame * f, struct spage * page);
struct lock fevict;

// EXTERN - RDS
/* Swap Table RDS */
struct swap_t *swaptable;


/* A memory pool. */
struct pool
{
	struct lock lock;                   /* Mutual exclusion. */
	struct bitmap *used_map;            /* Bitmap of free pages. */
	uint8_t *base;                      /* Base of pool. */
	size_t size;						/* Size of pool */
	size_t index;						/* Index for framelist */
	struct frame** framelist;           /* Array of frames */
};

/* Two pools: one for kernel data, one for user pages. */
static struct pool kernel_pool, user_pool;

static void init_pool (struct pool *, void *base, size_t page_cnt, const char *name);
static bool page_from_pool (const struct pool *, void *page);

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
	void
palloc_init (size_t user_page_limit)
{
	lock_init(&fevict);
	/* Free memory starts at 1 MB and runs to the end of RAM. */
	uint8_t *free_start = ptov (1024 * 1024);
	uint8_t *free_end = ptov (init_ram_pages * PGSIZE);
	size_t free_pages = (free_end - free_start) / PGSIZE;
	size_t user_pages = free_pages / 2;
	size_t kernel_pages;
	if (user_pages > user_page_limit)
		user_pages = user_page_limit;
	kernel_pages = free_pages - user_pages;

	/* Give half of memory to kernel, half to user. */
	init_pool (&kernel_pool, free_start, kernel_pages, "kernel pool");
	init_pool (&user_pool, free_start + kernel_pages * PGSIZE, user_pages, "user pool");

	// Initialize Frame List - rds
	user_pool.framelist = (struct frame **) calloc(user_pages, sizeof(struct frame *));
	user_pool.index = 0;
}


void *frame_selector (void* upage, enum palloc_flags flags)
{
	struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
	size_t page_idx;
	void *pages = NULL;

	while(pages == NULL)
	{
		lock_acquire (&pool->lock);
		page_idx = bitmap_scan_and_flip (pool->used_map, 0, 1, false);
		lock_release (&pool->lock);

		if (page_idx != BITMAP_ERROR)
			pages = pool->base + PGSIZE * page_idx;
		else
			pages = NULL;

		if (pages != NULL) 
		{
			if (flags & PAL_ZERO)
			{
				memset (pages, 0, PGSIZE);
			}

			struct frame * f = (struct frame *) malloc(sizeof(struct frame));
			f->t = thread_current();
			f->upage = upage;
			f->kpage = pages;

			if(pool->framelist[page_idx] != NULL)
			{
				free(pool->framelist[page_idx]);
			}
			pool->framelist[page_idx] = f;
		}
		else 
		{
			// Frame Eviction
			pages = frame_eviction(pool->framelist, flags, upage);
		}
	}

	return pages;
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
	void *
palloc_get_multiple (enum palloc_flags flags, size_t page_cnt)
{
	struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
	size_t page_idx;
	void *pages;

	if (page_cnt == 0)
		return NULL;

	lock_acquire (&pool->lock);
	page_idx = bitmap_scan_and_flip (pool->used_map, 0, page_cnt, false);
	lock_release (&pool->lock);

	if (page_idx != BITMAP_ERROR)
		pages = pool->base + PGSIZE * page_idx;
	else
		pages = NULL;

	if (pages != NULL) 
	{
		if (flags & PAL_ZERO)
		{
			memset (pages, 0, PGSIZE * page_cnt);
		}
	}
	else 
	{
		if (flags & PAL_ASSERT)
			PANIC ("palloc_get: out of pages");
	}

	return pages;
}

// Second Chance Page Replacement Algorithm
void* frame_eviction(struct frame** framelist, enum palloc_flags flags, void* upage)
{
	if(!lock_held_by_current_thread (&fevict))
		lock_acquire(&fevict);

	bool found = false;
	while(!found)
	{		
		if(framelist[user_pool.index] == NULL)
			return NULL;

		struct spage * page = spage_lookup(&framelist[user_pool.index]->t->spagedir, framelist[user_pool.index]->upage);
		if(lock_try_acquire(&page->spagelock))
		{
			bool accessed = pagedir_is_accessed(framelist[user_pool.index]->t->pagedir, framelist[user_pool.index]->upage);
			bool dirty = pagedir_is_dirty(framelist[user_pool.index]->t->pagedir, framelist[user_pool.index]->upage);
			if(accessed && dirty) // Used and Modified
			{
				pagedir_set_accessed(framelist[user_pool.index]->t->pagedir, framelist[user_pool.index]->upage, false);
			}
			else if(!accessed && dirty) // Not Used but Modified
			{
				write_dirty_page(framelist[user_pool.index], page);
			}
			else if(accessed && !dirty) // Used but Not Modified
			{
				pagedir_set_accessed(framelist[user_pool.index]->t->pagedir, framelist[user_pool.index]->upage, false);
			}
			else if(!accessed && !dirty) // Not Used or Modified
			{
				pagedir_clear_page(framelist[user_pool.index]->t->pagedir, framelist[user_pool.index]->upage); 

				// Update frame
				framelist[user_pool.index]->t = thread_current();
				framelist[user_pool.index]->upage = upage;

				if(flags & PAL_ZERO)
					memset (framelist[user_pool.index]->kpage, 0, PGSIZE);

				found = true;
			}
			lock_release(&page->spagelock);
		}

		++user_pool.index;
		if(user_pool.index == user_pool.size)
			user_pool.index = 0;
	}

	if(user_pool.index == 0)
		return framelist[user_pool.size - 1]->kpage;
	else
		return framelist[user_pool.index - 1]->kpage;
	lock_release(&fevict);
}

void write_dirty_page(struct frame * f, struct spage * page)
{
	static uint32_t * ERROR_ADDR = 0xCCCCCCCC;
	lock_release(&fevict);
	pagedir_clear_page(f->t->pagedir, f->upage); 

	page->state = SWAP; 
	swap_write(swaptable, f->kpage, &page->swapindex); 

	// Panic Kernel if no more swap space 
	ASSERT(page->swapindex != BITMAP_ERROR); 
	if(f->t->pagedir != ERROR_ADDR)
	{
		pagedir_set_dirty(f->t->pagedir, f->upage, false);
		if(!pagedir_set_page(f->t->pagedir, f->upage, f->kpage, page->readonly))
		{
			ASSERT(false);
		}
	}
	lock_acquire(&fevict);
}

/* Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
	void *
palloc_get_page (enum palloc_flags flags) 
{
	return palloc_get_multiple (flags, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
	void
palloc_free_multiple (void *pages, size_t page_cnt) 
{
	bool upool = false;
	struct pool *pool;
	size_t page_free_idx;

	ASSERT (pg_ofs (pages) == 0);
	if (pages == NULL || page_cnt == 0)
		return;

	if (page_from_pool (&kernel_pool, pages))
	{
		pool = &kernel_pool;
	}
	else if (page_from_pool (&user_pool, pages))
	{ 
		upool = true;
		pool = &user_pool;
	}
	else
	{
		NOT_REACHED ();
	}

	page_free_idx = pg_no (pages) - pg_no (pool->base);

	if(upool)
	{
		if(page_free_idx > 0 && pool->framelist[page_free_idx] != NULL)
		{
			struct spage * page = spage_lookup(&pool->framelist[page_free_idx]->t->spagedir, pool->framelist[page_free_idx]->upage);

			lock_acquire(&page->spagelock);
			if(page->swapindex != BITMAP_ERROR)
			{
				swap_delete(swaptable, page->swapindex);
			}
			lock_release(&page->spagelock);

			free(pool->framelist[page_free_idx]);
			pool->framelist[page_free_idx] = NULL;
		}
		else
		{
			return;
		}
	}

#ifndef NDEBUG
	memset (pages, 0xcc, PGSIZE * page_cnt);
#endif

	ASSERT (bitmap_all (pool->used_map, page_free_idx, page_cnt));
	bitmap_set_multiple (pool->used_map, page_free_idx, page_cnt, false);
}

/* Frees the page at PAGE. */
	void
palloc_free_page (void *page) 
{
	palloc_free_multiple (page, 1);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
	static void
init_pool (struct pool *p, void *base, size_t page_cnt, const char *name) 
{
	/* We'll put the pool's used_map at its base.
	   Calculate the space needed for the bitmap
	   and subtract it from the pool's size. */
	size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (page_cnt), PGSIZE);
	if (bm_pages > page_cnt)
		PANIC ("Not enough memory in %s for bitmap.", name);
	page_cnt -= bm_pages;

	printf ("%zu pages available in %s.\n", page_cnt, name);

	/* Initialize the pool. */
	lock_init (&p->lock);
	p->used_map = bitmap_create_in_buf (page_cnt, base, bm_pages * PGSIZE);
	p->base = base + bm_pages * PGSIZE;
	p->size = page_cnt;
}

/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
	static bool
page_from_pool (const struct pool *pool, void *page) 
{
	size_t page_no = pg_no (page);
	size_t start_page = pg_no (pool->base);
	size_t end_page = start_page + bitmap_size (pool->used_map);

	return page_no >= start_page && page_no < end_page;
}

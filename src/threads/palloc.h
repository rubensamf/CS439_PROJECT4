#ifndef THREADS_PALLOC_H
#define THREADS_PALLOC_H

#include <stddef.h>
#include "lib/kernel/list.h"
#include "threads/thread.h"

extern struct lock fevict;

/* How to allocate pages. */
enum palloc_flags
  {
    PAL_ASSERT = 001,           /* Panic on failure. */
    PAL_ZERO = 002,             /* Zero page contents. */
    PAL_KERNEL = 003,           /* Kernel page. */
    PAL_USER = 004              /* User page. */
  };

struct frame
{
	struct thread * t;
	void* upage;
	void* kpage;
};

void palloc_init (size_t user_page_limit);
void *palloc_get_page (enum palloc_flags);
void *frame_selector (void* upage, enum palloc_flags);
void *palloc_get_multiple (enum palloc_flags, size_t page_cnt);
void palloc_free_page (void *);
void palloc_free_multiple (void *, size_t page_cnt);

#endif /* threads/palloc.h */

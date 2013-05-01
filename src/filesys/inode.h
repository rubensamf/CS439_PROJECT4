#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "lib/kernel/list.h"
#include "filesys/off_t.h"
#include "devices/block.h"

#define INODE_ERROR SIZE_MAX

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
	off_t pos;		            /* Current Position in Double Indirect Pointer */ 
	off_t size;                 /* File size in bytes - allocated by number of file sectors. */
	off_t length;               /* File size in bytes - written. */
	block_sector_t ptr;	        /* Double Indirect Pointer */

	// Directory
	bool is_directory;          /* Directory Flag */
	block_sector_t parent_dir;  /* Sector ID of parent directory */
	off_t count;				/* Number of files in directory */

	unsigned magic;             /* Magic number. */
	uint32_t unused[120];       /* Not used. */
};

/* In-memory inode. */
struct inode 
{
	struct list_elem elem;              /* Element in inode list. */
	block_sector_t sector;              /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

struct bitmap;

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool, block_sector_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */

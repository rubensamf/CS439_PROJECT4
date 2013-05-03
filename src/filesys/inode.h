// Turnin Date: 5/3/2013
//Name: Ryan Spring
//EID: rds2367
//CS login: rdspring
//Email: rdspring1@gmail.com
//Unique Number: 53426

//Name1: Ruben Fitch
//EID1: rsf293
//CS login: rubensam
//Email: rubensamf@utexas.edu
//Unique Number: 53435

//Name2: Josue Roman
//EID2: jgr397
//CS login: jroman
//Email: jozue.roman@gmail.com
//Unique Number: 53425

//Name3: Peter Farago
//EID2: pf3546
//CS login: farago
//Email: faragopeti@tx.rr.com
//Unique Number: 53435

#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "threads/synch.h"
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
	block_sector_t parent_dir;  /* Inode Pointer of parent directory */
	off_t count;				/* Number of files in directory */
	off_t wdir;					/* Number of places where this directory is the current working directory for a process */
        
	unsigned magic;             /* Magic number. */
	uint32_t unused[119];       /* Not used. */
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
	struct lock inode_lock;				/* Inode Lock for inode synchronization */
	struct lock dir_lock;				/* Inode Lock for directory synchronization */
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
void inode_lock (const struct inode *inode);
void inode_unlock (const struct inode *inode);

#endif /* filesys/inode.h */

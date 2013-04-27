#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

// MULTILEVEL FILE - RDS
#define MLSIZE 128
#define INODE_ERROR SIZE_MAX

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
	off_t pos;		            /* Current Position in Double Indirect Pointer */ 
	off_t length;               /* File size in bytes. */
	block_sector_t ptr;	        /* Double Indirect Pointer */
	bool is_directory;          /* Directory Flag */
	unsigned magic;             /* Magic number. */
	uint32_t unused[123];       /* Not used. */
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
	static inline size_t
bytes_to_sectors (off_t size)
{
	return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

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

// New Functions - RDS
void inode_release(block_sector_t ptr);
bool inode_extend(struct inode *inode, off_t offset);
bool inode_allocate(struct inode_disk* disk_inode, off_t size, block_sector_t* dli, block_sector_t* sli);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
	static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
	ASSERT (inode != NULL);
	if (pos < inode->data.length)
	{
		off_t sector = pos / BLOCK_SECTOR_SIZE;
		off_t dli_pos = sector / MLSIZE;
		off_t sli_pos = sector % MLSIZE;

		block_sector_t* dli = malloc(MLSIZE * sizeof(block_sector_t));
		if(dli == NULL)
			return INODE_ERROR;

		block_sector_t* sli = malloc(MLSIZE * sizeof(block_sector_t));
		if(sli == NULL)
			return INODE_ERROR;

		block_read(fs_device, inode->data.ptr, dli);
		block_read(fs_device, dli[dli_pos], sli);
		return sli[sli_pos];
	}
	else
	{
		return INODE_ERROR;
	}
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
	void
inode_init (void) 
{
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
	bool
inode_create (block_sector_t sector, off_t length)
{
	struct inode_disk *disk_inode = NULL;
	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	   one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL)
	{
		disk_inode->pos = 0;
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;

		if (!free_map_allocate (1, &disk_inode->ptr)) 
		{
			free (disk_inode);
			return false;
		}

		block_sector_t* dli = malloc(MLSIZE * sizeof(block_sector_t));
		if(dli == NULL)
		{
			free (disk_inode);
		}

		block_sector_t* sli = malloc(MLSIZE * sizeof(block_sector_t));
		if(sli == NULL)
		{
			free (disk_inode);
			free (dli);
		}

		memset (dli, INODE_ERROR, MLSIZE * sizeof(block_sector_t));
		memset (sli, INODE_ERROR, MLSIZE * sizeof(block_sector_t));

		bool result = inode_allocate(disk_inode, 0, dli, sli);

		if(!result)
		{
			inode_release (disk_inode->ptr);
			free (sli);
			free (dli);
			free (disk_inode);
			return false;
		}
		else
		{
			block_write (fs_device, disk_inode->ptr, dli);
			block_write (fs_device, sector, disk_inode);
			free (sli);
			free (dli);
			free (disk_inode);
		}
	}
	return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
	struct inode *
inode_open (block_sector_t sector)
{
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) 
	{
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) 
		{
			inode_reopen (inode);
			return inode; 
		}
	}

	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	block_read (fs_device, inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
	struct inode *
inode_reopen (struct inode *inode)
{
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
	block_sector_t
inode_get_inumber (const struct inode *inode)
{
	return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
	void
inode_close (struct inode *inode) 
{
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0)
	{
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

		/* Deallocate blocks if removed. */
		if (inode->removed) 
		{
			free_map_release (inode->sector, 1);
			// Replace with function to release every allocated page for inode
			inode_release (inode->data.ptr);
		}
		free (inode); 
	}
}

void inode_release(block_sector_t ptr)
{
	block_sector_t* dli = malloc(MLSIZE * sizeof(block_sector_t));
	if(dli == NULL)
		return;

	block_read(fs_device, ptr, dli);
	free_map_release(ptr, 1);

	block_sector_t* sli = malloc(MLSIZE * sizeof(block_sector_t));
	if(sli == NULL)
		return;

	unsigned i;
	unsigned j;
	for(i = 0; i < MLSIZE; ++i)
	{
		if(dli[i] == INODE_ERROR)
		{
			free(dli);
			free(sli);
			return;
		}

		block_read(fs_device, dli[i], sli);
		free_map_release(dli[i], 1);

		for(j = 0; j < MLSIZE; ++j)
		{
			if(sli[j] == INODE_ERROR)
			{
				free(dli);
				free(sli);
				return;
			}
			free_map_release(sli[j], 1);
		}
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
	void
inode_remove (struct inode *inode) 
{
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
	off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	//printf("---------INODE READ: %d:%d---------\n", inode->data.ptr, size);
	//debug_backtrace();
	while (size > 0) 
	{
		/* Disk sector to read, starting byte offset within sector. */
		block_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
		{
			/* Read full sector directly into caller's buffer. */
			block_read (fs_device, sector_idx, buffer + bytes_read);
		}
		else 
		{
			/* Read sector into bounce buffer, then partially copy
			   into caller's buffer. */
			if (bounce == NULL) 
			{
				bounce = malloc (BLOCK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			block_read (fs_device, sector_idx, bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;

	}
	free (bounce);
	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
	off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) 
{
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt)
		return 0;

	//printf("--------------INODE WRITE: %d:%d:%d:%d--------------\n", inode->data.ptr, offset, size, buffer[0]);
	//debug_backtrace();
	while (size > 0) 
	{
		/* Sector to write, starting byte offset within sector. */
		block_sector_t sector_idx = byte_to_sector (inode, offset);
		if(sector_idx == INODE_ERROR)
		{
			if(!inode_extend(inode, offset + size))
				return false;
			else
				sector_idx = byte_to_sector (inode, offset);              
		}

		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
		{
			/* Write full sector directly to disk. */
			block_write (fs_device, sector_idx, buffer + bytes_written);
		}
		else 
		{
			/* We need a bounce buffer. */
			if (bounce == NULL) 
			{
				bounce = malloc (BLOCK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				block_read (fs_device, sector_idx, bounce);
			else
				memset (bounce, 0, BLOCK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			block_write (fs_device, sector_idx, bounce);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free (bounce);
	return bytes_written;
}

bool inode_extend(struct inode *inode, off_t size)
{
	struct inode_disk disk_inode = inode->data;
	off_t length = inode_length (inode);
	off_t last_sector = length / BLOCK_SECTOR_SIZE;
	off_t dli_pos = last_sector / MLSIZE;

	block_sector_t* dli = malloc(MLSIZE * sizeof(block_sector_t));
	if(dli == NULL)
		return false;

	block_sector_t* sli = malloc(MLSIZE * sizeof(block_sector_t));
	if(sli == NULL)
		return false;

	block_read(fs_device, disk_inode.ptr, dli);
	block_read(fs_device, dli[dli_pos], sli);

	bool result = inode_allocate(&disk_inode, size, dli, sli);
	free (sli);
	free (dli);

	if(!result)
		return false;
	else
		return true;
}

bool inode_allocate(struct inode_disk* disk_inode, off_t size, block_sector_t* dli, block_sector_t* sli)
{
	off_t length = disk_inode->length;
	size_t sectors;
	for(sectors = bytes_to_sectors (length + size); sectors > 0; --sectors)
	{
		static char zeros[BLOCK_SECTOR_SIZE];
		if (free_map_allocate (1, &sli[disk_inode->pos])) 
		{
			block_write (fs_device, sli[disk_inode->pos], zeros);
		}
		else
		{	
			return false;
		}

		++disk_inode->pos;
		if(disk_inode->pos % MLSIZE == 0)
		{
			off_t dli_pos = disk_inode->pos / MLSIZE;
			bool add = free_map_allocate (1, &dli[dli_pos]);
			if(add)
			{
				block_write (fs_device, dli[dli_pos], sli);	
				memset (sli, INODE_ERROR, MLSIZE * sizeof(block_sector_t));
			}
			else
			{
				return false;
			}
		}
	}

	if(disk_inode->pos % MLSIZE != 0)
	{
		off_t dli_pos = disk_inode->pos / MLSIZE;
		bool add = free_map_allocate (1, &dli[dli_pos]);
		if(add)
		{
			block_write (fs_device, dli[dli_pos], sli);	
		}
		else
		{
			return false;
		}
	}
	return true;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
	void
inode_deny_write (struct inode *inode) 
{
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
	void
inode_allow_write (struct inode *inode) 
{
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
	off_t
inode_length (const struct inode *inode)
{
	return inode->data.length;
}

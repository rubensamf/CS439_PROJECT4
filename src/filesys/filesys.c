#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
	void
filesys_init (bool format) 
{
	fs_device = block_get_role (BLOCK_FILESYS);
	if (fs_device == NULL)
		PANIC ("No file system device found, can't initialize file system.");

	inode_init ();
	free_map_init ();

	if (format) 
		do_format ();

	free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
	void
filesys_done (void) 
{
	free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
	bool
filesys_create (const char *name, off_t initial_size) 
{
	struct list * path = parse_filepath((char*) name);
	struct dir * dir = navigate_filesys(path, (char*) name, true);
	struct list_elem * e = list_back(path);
	struct path * p = list_entry(e, struct path, elem);

	block_sector_t inode_sector = 0;
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size, false, INODE_ERROR)
			&& dir_add (dir, p->path, inode_sector));
	if (!success && inode_sector != 0) 
		free_map_release (inode_sector, 1);
	dir_close (dir);

	delete_pathlist(path);
	return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
	struct file *
filesys_open (const char *name)
{
	struct list * path = parse_filepath((char*) name);
	struct dir * dir = navigate_filesys(path, (char*) name, true);
	struct list_elem * e = list_back(path);
	struct path * p = list_entry(e, struct path, elem);
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, p->path, &inode);
	dir_close (dir);

	delete_pathlist(path);
	return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
	bool
filesys_remove (const char *name) 
{
	struct list * path = parse_filepath((char*) name);
	struct dir * dir = navigate_filesys(path, (char*) name, true);
	struct list_elem * e = list_back(path);
	struct path * p = list_entry(e, struct path, elem);

	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir); 

	delete_pathlist(path);
	return success;
}

/* Formats the file system. */
	static void
do_format (void)
{
	printf ("Formatting file system...");
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, DIRSIZE, ROOT_DIR_SECTOR))
		PANIC ("root directory creation failed");
	free_map_close ();
	printf ("done.\n");
}

/* Navigate the filesys to the current directory; otherwise return NULL */
struct dir * navigate_filesys(struct list* path, char* filepath, bool file)
{
	static char check[] = "/";
	static char cdir[] = ".";
	static char prevdir[] = "..";

	if(list_size(path) <= 0)
		return NULL;

	struct dir * directory = NULL;
	if(filepath[0] == check[0])
	{
		directory = dir_open_root();
	}
	else if(thread_current()->filedir == 0)
	{
		thread_current()->filedir = ROOT_DIR_SECTOR;
		directory = dir_open_root();
	}
	else
	{
		directory = dir_open(inode_open(thread_current()->filedir));
	}

	size_t length;
	size_t i;
	struct inode * inode = NULL;
	struct list_elem * e = list_begin(path);

	if(file)
		length = list_size(path) - 1;
	else
		length = list_size(path);

	for(i = 0; i < length; ++i)
	{
		struct path * p = list_entry(e, struct path, elem);

		// Navigate file system
		if(strcmp(p->path, prevdir) == 0)
		{
			dir_close(directory);
			directory = dir_open(inode_open(inode->data.parent_dir));	
		}
		else if(dir_lookup(directory, p->path, &inode) && inode->data.is_directory)
		{
			dir_close(directory);
			directory = dir_open(inode);
		}
		else if(strcmp(p->path, cdir) != 0)
		{
			return NULL;
		}
		e = list_next(e);
	}
	return directory;
}

/* Parse the filepath into a list */
struct list* parse_filepath(char* filepath)
{
	struct list* path = (struct list*) malloc(sizeof(struct list));
	list_init(path);

	char *fp_copy, *token, *save_ptr;

	/* Make a copy of Filepath. */
	fp_copy = palloc_get_page (0);
	if (fp_copy == NULL)
		return NULL;
	strlcpy (fp_copy, filepath, PGSIZE);

	for (token = strtok_r (fp_copy, "/", &save_ptr); token != NULL; token = strtok_r (NULL, "/", &save_ptr))
	{
		struct path * p = (struct path *) malloc(sizeof(struct path));
		p->path = (char *) malloc(strlen(token) + 1);
		strlcpy (p->path, token, strlen(token) + 1);
		list_push_back(path, &p->elem);
	}
	palloc_free_page (fp_copy);
	return path;
}

/* Delete the list containing the entries of the filepath */
void delete_pathlist(struct list* list)
{
	while(!list_empty(list))
	{
		struct list_elem * e = list_pop_front(list);
		struct path * p = list_entry(e, struct path, elem);
		free(p);
	}
	free(list);
}

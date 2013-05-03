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

#include <debug.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/fdt.h"
#include <stdio.h>

/* Slots 0 and 1 in the array of struct file * should always be
   reserved for stdin and stdout. I'm unsure of how to actually
   treat them as files proper so those addresses are NULL for
   the time being. BDH */

/* Locates the first non-null slot in the array and returns
   the index to that slot as the file descriptor. Returns -1
   in case no free slots were found.

   If memory constraints are a problem, this code could be rewritten
   to allocate arrays and then "enlarge" them as needed, not unlike
   a Java ArrayList. The process would be transparent to the caller. */
int fd_create(struct file *file)
{
  if (file == NULL)
    return -1;

  int i;
  fdt_t fdt = thread_current()->fdt;

  // initialized to 2 "leave space" for stdin and stdout
  for (i = 2; i < FDT_MAX_FILES; i++)
  {
    	if (fdt[i] == 0) {
			fdt[i] = file;
			return i;
		}
  }

  return -1; // completely filled array
}

/* Returns the file associated with the given descriptor */
struct file *fd_get_file(int fd)
{
  // Return null in case of a problem
  if (fd < 0 || FDT_MAX_FILES <= fd)
    return NULL;

  return thread_current()->fdt[fd];
}

/* Sets the index fd to NULL and returns the file associated with
   it. It should be noted that closing the file itself is the caller's
   responsibility. */
struct file *fd_remove(int fd)
{
  if(fd < 0 || fd >= FDT_MAX_FILES)
	  return NULL;

  fdt_t fdt = thread_current()->fdt;
  struct file *file = fdt[fd];

  fdt[fd] = 0;
  return file;
}

/* Closes all files (except stdin and stdout) and frees memory. */
void fdt_destroy(fdt_t fdt)
{
  if (fdt == 0)
    return;

  int i;

  for (i = 2; i < FDT_MAX_FILES; i++)
    if (fdt[i] != 0)
      file_close(fdt[i]);

  free(fdt);
}

/* Creates a new null-initialized file descriptor table. We are
   making a slight assumption that NULL is indeed 0. */
fdt_t fdt_init()
{
	return (fdt_t) calloc(FDT_MAX_FILES, sizeof(struct file *));
}

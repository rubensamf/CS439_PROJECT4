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

#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"
#include "filesys/directory.h"

struct inode;

/* An open file. */
struct file 
{
	struct inode *inode;        /* File's inode. */
	struct dir *dir;			/* The dir struct if the file is a directory */
	off_t pos;                  /* Current position. */
	bool deny_write;            /* Has file_deny_write() been called? */
};

/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

#endif /* filesys/file.h */

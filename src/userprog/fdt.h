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

#ifndef USERPROG_FDT_H
#define USERPROG_FDT_H

#define FDT_MAX_FILES 128

typedef struct file ** fdt_t;

int fd_create(struct file *file);
struct file *fd_get_file(int fd);
struct file *fd_remove(int fd);

void fdt_destroy(fdt_t fdt);
fdt_t fdt_init(void);

#endif

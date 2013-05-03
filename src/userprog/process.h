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

#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "devices/block.h"

extern uint8_t * stack_bound; 

tid_t process_execute (const char *file_name, block_sector_t filedir);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void addChildProc(tid_t childid);
bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */

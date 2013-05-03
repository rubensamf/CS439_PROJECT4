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

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "threads/synch.h"

extern struct list waitproc_list;
extern struct semaphore exec_load_sema;
extern bool exec_load_status;

struct waitproc
{
	struct semaphore sema;
	tid_t id;
	struct list_elem elem; 
};

void syscall_init (void);

typedef void exit_action_func (struct exitstatus * es, void *aux);
void exit_foreach(exit_action_func * es, void * aux);

void sysexit(int status);
#endif /* userprog/syscall.h */

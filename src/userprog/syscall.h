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

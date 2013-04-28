#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

extern uint8_t * stack_bound; 

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void addChildProc(tid_t childid);
bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */

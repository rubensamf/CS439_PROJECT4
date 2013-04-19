#include <stdio.h>
#include <stdint.h>
#include <syscall-nr.h>
#include <string.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/fdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#define user_return(val) frame->eax = val; return
#define MAX_SIZE 256

// Extern
struct list exit_list;
struct list waitproc_list;
struct semaphore exec_load_sema;
bool exec_load_status;

// GLOBALS
struct list ignore_list;

const unsigned CONSOLEWRITE = 1;
const unsigned CONSOLEREAD = 0;

static void syscall_handler (struct intr_frame* frame);
bool exit_remove(tid_t id);
bool ignore_remove(tid_t id);

// User Memory Check
static bool check_uptr(const void* uptr);
static bool check_buffer(const char* uptr, unsigned length);
static uintptr_t next_value(uintptr_t** sp);
static char* next_charptr(uintptr_t** sp);
static void* next_ptr(uintptr_t** sp);

// Locks
static struct lock exec_lock;
static struct lock filecreate_lock;
static struct lock fileremove_lock;

// Syscall Functions
static void sysclose(int fd);
static void syscreate(struct intr_frame* frame, const char* file, unsigned size);
static void sysexec(struct intr_frame* frame, const char* file);
static void sysfilesize(struct intr_frame *frame, int fd);
static void sysopen(struct intr_frame *frame, const char *file);
static void sysread(struct intr_frame *frame, int fd, void *buffer, unsigned size);
static void sysremove(struct intr_frame* frame, const char* file);
static void sysseek(int fd, unsigned position);
static void systell(struct intr_frame *frame, int fd);
static void syswrite(struct intr_frame *frame, int fd, const void *buffer, unsigned size);

/* Determine whether user process pointer is valid;
   Otherwise, return false*/ 
static bool
check_uptr (const void* uptr)
{
	if(uptr != NULL)
	{
		if(is_user_vaddr(uptr))
		{
			void* kptr = pagedir_get_page(thread_current()->pagedir, uptr);
			if(kptr != NULL)
			{
				return true;
			}
		}
	} 
	return false;
}

static bool
check_buffer (const char* uptr, unsigned length)
{
	unsigned i;
	for(i = 0; i < length; ++i)
	{
		if(!check_uptr(uptr))
			return false;
		++uptr;
	}
	return true;
}

void
syscall_init (void) 
{
	list_init (&ignore_list);
	list_init (&waitproc_list);
	list_init (&exit_list);

	// Initialize Private Locks
	sema_init(&exec_load_sema, 0);
	lock_init(&exec_lock);
	lock_init(&filecreate_lock);
	lock_init(&fileremove_lock);

	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame* frame) 
{
	// -------- System Call Handler Overview -------- 
	// Get system call number
	// switch statement using system call number
	// collect arguments for system call function if necessary
	// call system call function
	// set frame->eax to return value if necessary
	// ----------------------------------------------
	uintptr_t* kpaddr_sp = (uintptr_t*) frame->esp;
	int syscall_num = -1;
	if(check_uptr(kpaddr_sp))
		syscall_num = next_value(&kpaddr_sp);
	else
		sysexit(-1);

	switch(syscall_num)
	{
		case SYS_HALT:                   
			{
				// Terminates Pintos
				shutdown_power_off();
			}
			break;
		case SYS_EXIT:                 
			{
				uintptr_t status = -1;
				if(check_uptr(kpaddr_sp))
					status = next_value(&kpaddr_sp);
				sysexit(status);
			}
			break;
		case SYS_EXEC:  //pid_t exec (const char *file);
			{
				const char* file =  next_charptr(&kpaddr_sp);
				if(file == NULL)
					sysexit(-1);

				unsigned len = strlen(file);
				if(!check_buffer(file, len))
					sysexit(-1);
				else
					sysexec(frame, file);
			}
			break;
		case SYS_WAIT:  //int wait (pid_t);
			{
				uintptr_t childid = -1;
				if(check_uptr(kpaddr_sp))
					childid = next_value(&kpaddr_sp);
				else
					sysexit(childid);
			
				int retval = process_wait((tid_t) childid);
				frame->eax = retval;
			}
			break;
		case SYS_CREATE:	//bool create (const char *file, unsigned initial_size);
			{
				const char* file =  next_charptr(&kpaddr_sp);
				if(file == NULL)
					sysexit(-1);

				unsigned len = strlen(file);
				if(!check_buffer(file, len))
					sysexit(-1);

				uintptr_t size = 0;
				if(check_uptr(kpaddr_sp))
					size = next_value(&kpaddr_sp);
				else
					sysexit(-1);

				syscreate(frame, file, size);
			}
			break;
		case SYS_REMOVE:	//bool remove (const char *file);
			{
				const char* file =  next_charptr(&kpaddr_sp);
				if(file == NULL)
					sysexit(-1);

				unsigned len = strlen(file);
				if(!check_buffer(file, len))
					sysexit(-1);

				sysremove(frame, file);
			}
			break;
		case SYS_OPEN:          
			{
				//int open (const char *file);
				const char* file =  next_charptr(&kpaddr_sp);
				if(file == NULL)
					sysexit(-1);

				unsigned len = strlen(file);
				if(!check_buffer(file, len))
					sysexit(-1);

	      		sysopen(frame, file);
			}
			break;
		case SYS_FILESIZE:     
			{
				//int filesize (int fd);
	      		int fd = 0;
	      		if (check_uptr(kpaddr_sp))
					fd = (int) next_value(&kpaddr_sp);
	      		else
					sysexit(-1);

	      		sysfilesize(frame, fd);
			}
			break;
		case SYS_READ:        
			{
				//int read (int fd, void *buffer, unsigned length);
				int fd = 0;
				if (check_uptr(kpaddr_sp))
					fd = (int) next_value(&kpaddr_sp);
				else
					sysexit(-1);

				const char* file =  next_charptr(&kpaddr_sp);
				if(file == NULL)
					sysexit(-1);

				unsigned len = strlen(file);
				if(!check_buffer(file, len))
					sysexit(-1);

				unsigned length = 0;
				if (check_uptr(kpaddr_sp))
					length = (unsigned) next_value(&kpaddr_sp);
				else
					sysexit(-1);

				sysread(frame, fd, (void*) file, length);
			}
			break;
		case SYS_WRITE:      
			{
				//int write (int fd, const void *buffer, unsigned length);
				uintptr_t fd = 0;
				if(check_uptr(kpaddr_sp))
					fd = next_value(&kpaddr_sp);
				else
					sysexit(-1);

				const char* file =  next_charptr(&kpaddr_sp);
				if(file == NULL)
					sysexit(-1);

				unsigned len = strlen(file);
				if(!check_buffer(file, len))
					sysexit(-1);

				uintptr_t length = 0;
				if(check_uptr(kpaddr_sp))
					length = next_value(&kpaddr_sp);
				else
					sysexit(-1);

				if(fd == CONSOLEWRITE) // Write to Console
				{
					while(length > 0)
					{
						if(length > MAX_SIZE)
						{
							putbuf (file, MAX_SIZE);
							file += MAX_SIZE;
							length -= MAX_SIZE;
						}
						else
						{
							putbuf (file, length);
							length = 0;
						}
					}
				}
				else
				{
					syswrite(frame, fd, file, length);
				}
			}
			break;
		case SYS_SEEK:
			{
				//void seek (int fd, unsigned position);
				int fd = 0;
				if (check_uptr(kpaddr_sp))
					fd = (int) next_value(&kpaddr_sp);
				else
					sysexit(-1);

				unsigned position = 0;
				if (check_uptr(kpaddr_sp))
					position = (unsigned) next_value(&kpaddr_sp);
				else
					sysexit(-1);

				sysseek(fd, position);
			}
			break;
		case SYS_TELL:
			{
				//unsigned tell (int fd);
				int fd = 0;
				if (check_uptr(kpaddr_sp))
					fd = (int) next_value(&kpaddr_sp);
				else
					sysexit(-1);

				systell(frame, fd);
			}
			break;
		case SYS_CLOSE:    
			{
				//void close (int fd);
				int fd = 0;
				if (check_uptr(kpaddr_sp))
					fd = (int) next_value(&kpaddr_sp);
				else
					sysexit(-1);

				sysclose(fd);
			}
			break;
		default:
			{
				printf("Unrecognized System Call\n");
				sysexit(-1);
			}
			break;
	}
}

static uintptr_t
next_value(uintptr_t** sp)
{
	uintptr_t* ptr = *sp;
	uintptr_t value = *ptr;
	++ptr;
	*sp = ptr;
	return value;
}


static void*
next_ptr(uintptr_t** sp)
{
	void* voidptr = (void*) next_value(sp);
	if(check_uptr(voidptr))
		return voidptr;
	else
		return NULL;
}

static char*
next_charptr(uintptr_t** sp)
{
	return (char *) next_ptr(sp);
}

void
sysexit(int status)
{
	// Print Process Termination Message
	// File Name	
	char* name = thread_current()->name;
	char* token, *save_ptr;
	token = strtok_r(name, " ", &save_ptr);
	putbuf (token, strlen(token));

	char* str1 = ": exit(";
	putbuf (str1, strlen(str1));

	// ExitStatus
	char strstatus[32];
	snprintf(strstatus, 32, "%d", status);
	putbuf (strstatus, strlen(strstatus));

	char* str2 = ")\n";
	putbuf (str2, strlen(str2));

	// EXIT Child Processes
	if(thread_current()->numchild > 0)
	{
		struct list_elem * e;
		while (!list_empty(&thread_current()->child_list))
		{
			e = list_pop_front(&thread_current()->child_list);
			struct childproc * childitem = list_entry (e, struct childproc, elem);
			if(!exit_remove(childitem->childid))
			{
				list_push_back(&ignore_list, &childitem->elem);
			}
			else
			{
				free(childitem);
			}
		}
	}

	// Save exit status
	struct exitstatus * es = (struct exitstatus *) malloc(sizeof(struct exitstatus));
	if(es != NULL && !ignore_remove(thread_current()->tid))
	{
		es->avail = true;
		es->status = status;
		es->childid = thread_current()->tid;
		list_push_back(&exit_list, &es->elem);

		struct list_elem * e;
		for (e = list_begin (&waitproc_list); e != list_end (&waitproc_list); e = list_next (e))
		{
			struct waitproc * item = list_entry (e, struct waitproc, elem);
			sema_up(&item->sema);	
		}
	}
	thread_exit();
}

static void
sysclose(int fd)
{
	struct file *file = fd_remove(fd);
	if (file != NULL)
		file_close(file);
	else
		sysexit(-1);
}

static void
syscreate(struct intr_frame* frame, const char* file, unsigned size)
{
	lock_acquire(&filecreate_lock);
	bool result = filesys_create(file, size);
	frame->eax = result;
	lock_release(&filecreate_lock);
}

static void
sysexec(struct intr_frame* frame, const char* file)
{
	lock_acquire(&exec_lock);

	sema_init(&exec_load_sema, 0);
	tid_t newpid = process_execute(file);
	sema_down(&exec_load_sema);

	if(exec_load_status)
	{
		frame->eax = newpid;
		addChildProc(newpid);
	}
	else
	{
		frame->eax = TID_ERROR;
	}

	lock_release(&exec_lock);
}

static void
sysfilesize(struct intr_frame *frame, int fd)
{
	struct file *file = fd_get_file(fd);

	if (file == NULL) 
	{
		user_return(-1);
	}
	else 
	{
		user_return( file_length(file) );
	}
}

static void
sysopen(struct intr_frame *frame, const char *file)
{
	struct file *f = filesys_open(file);

	if (f == NULL) 
	{
		user_return(-1);
	}
	else 
	{
		user_return( fd_create(f) );
	}
}

static void
sysread(struct intr_frame *frame, int fd, void *buffer, unsigned size)
{
	// special case
	if (fd == STDIN_FILENO) 
	{
		char *char_buffer = (char *) buffer;

		while (size-- > 0) 
		{
			*char_buffer++ = input_getc();
		}

		user_return(size);
	}

	struct file *file = fd_get_file(fd);

	if (file == NULL) 
	{
		user_return(-1);
	}
	else 
	{
		user_return( file_read(file, buffer, size) );
	}
}

static void 
sysremove(struct intr_frame* frame, const char* file)
{
	lock_acquire(&fileremove_lock);
	bool result = filesys_remove(file);
	frame->eax = result;
	lock_release(&fileremove_lock);
}

static void
sysseek(int fd, unsigned position)
{
	struct file *file = fd_get_file(fd);

	if (file == NULL)
		return;
	else
		file_seek(file, position);
}

static void
systell(struct intr_frame *frame, int fd)
{
	struct file *file = fd_get_file(fd);

	if (file == NULL) 
	{
		user_return(-1);
	}
	else
	{
		user_return( (unsigned) file_tell(file) );
	}
}

static void
syswrite(struct intr_frame *frame, int fd, const void *buffer, unsigned size)
{
	struct file *file = fd_get_file(fd);

	if (file == NULL) 
	{
		user_return(-1);
	}
	else 
	{
		user_return( file_write(file, buffer, size) );
	}
}

bool exit_remove(tid_t id)
{
	struct list_elem * e;
	for (e = list_begin (&exit_list); e != list_end (&exit_list); e = list_next (e))
	{
		struct exitstatus * es = list_entry (e, struct exitstatus, elem);
		if(es->childid == id)
		{
			list_remove(e);
			free(es);
			return true;
		}
	}
	return false;
}

bool ignore_remove(tid_t id)
{
	struct list_elem * e;
	for (e = list_begin (&ignore_list); e != list_end (&ignore_list); e = list_next (e))
	{
		struct childproc * es = list_entry (e, struct childproc, elem);
		if(es->childid == id)
		{
			list_remove(es);
			free(es);
			return true;
		}
	}
	return false;
}

void 
exit_foreach(exit_action_func * func, void* aux)
{
	struct list_elem * e;
	for (e = list_begin (&exit_list); e != list_end (&exit_list); e = list_next (e))
	{
		struct exitstatus * es = list_entry (e, struct exitstatus, elem);
		func (es, aux);
	}
}

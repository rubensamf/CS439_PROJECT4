#include "userprog/process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/fdt.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/spage.h"
#include "lib/kernel/bitmap.h"

#define MAX_NAME_LEN 32
#define MAX_NUM_BYTES 4080

// Extern
struct semaphore exec_load_sema;
struct list waitproc_list;
bool exec_load_status;
uint8_t * stack_bound;
struct lock fevict;

// Additional Function Prototypes
static int count_bytes(char **str_ptr);
char** push_arguments(int num_bytes, char *str_ptr, const char *base);

bool validCTID(tid_t child_tid);
bool checkWaitList(tid_t child_tid);
bool checkCTID(tid_t child_tid);
int getCTID(tid_t child_tid);
void getExitStatus(struct exitstatus * es, void * aux);

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

struct pcreate
{
	const char* file_name;
	block_sector_t filedir;
};

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
	tid_t
process_execute (const char *file_name, block_sector_t filedir) 
{
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	   Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy); 
	else
		addChildProc(tid);
	return tid;
}

/* A thread function that loads a user process and starts it running. */
	static void
start_process (void *aux)
{
	char *file_name = aux;
	struct intr_frame if_;
	bool success;

	thread_current()->fdt = fdt_init();
	thread_current()->filedir = 1;

	/* Initialize interrupt frame and load executable. */
	memset (&if_, 0, sizeof if_);
	if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
	if_.cs = SEL_UCSEG;
	if_.eflags = FLAG_IF | FLAG_MBS;
	success = load (file_name, &if_.eip, &if_.esp);

	/* If load failed, quit. */
	palloc_free_page (file_name);
	if (!success) 
	{
		thread_exit ();
	}

	/* Start the user process by simulating a return from an
	   interrupt, implemented by intr_exit (in
	   threads/intr-stubs.S).  Because intr_exit takes all of its
	   arguments on the stack in the form of a `struct intr_frame',
	   we just point the stack pointer (%esp) to our stack frame
	   and jump to it. */
	asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
	NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
	int
process_wait (tid_t child_tid) 
{
	int retval = -1;

	// Check Validity of TID - A member of child list
	if(validCTID(child_tid))
	{
		// Check if TID is a member of wait list
		if(!checkWaitList(child_tid))
		{
			struct childproc * child = (struct childproc *) malloc(sizeof(struct childproc));
			if(child != NULL)
			{
				child->childid = child_tid;
				list_push_back(&thread_current()->wait_list, &child->elem);
			}

			struct waitproc * wp = (struct waitproc *) malloc(sizeof(struct waitproc));
			if(wp != NULL)
			{
				wp->id = thread_current()->tid;
				sema_init(&wp->sema, 0);
				list_push_back(&waitproc_list, &wp->elem);
				while(!checkCTID(child_tid))
				{
					sema_down(&wp->sema);
				}
			}
			list_remove(&wp->elem);
			free(wp);

			// Set retval to correct exit status
			retval = getCTID(child_tid);
		}
	}
	return retval;
}

/* Free the current process's resources. */
	void
process_exit (void)
{
	struct thread *cur = thread_current ();

	if(thread_current()->numchild > 0)
	{
		// Free every wait item of the process
		struct list_elem * e;
		while (!list_empty(&cur->wait_list))
		{
			e = list_pop_front(&cur->wait_list);
			struct childproc * childitem = list_entry (e, struct childproc, elem);
			free(childitem);
		}
	}

	file_close(thread_current()->file);
	fdt_destroy(cur->fdt);

	uint32_t *pd;
	/* Destroy the current process's page directory and switch back
	   to the kernel-only page directory. */
	pd = cur->pagedir;
	if (pd != NULL) 
	{
		/* Correct ordering here is crucial.  We must set
		   cur->pagedir to NULL before switching page directories,
		   so that a timer interrupt can't switch back to the
		   process page directory.  We must activate the base page
		   directory before destroying the process's page
		   directory, or our active page directory will be one
		   that's been freed (and cleared). */
		cur->pagedir = NULL;
		pagedir_activate (NULL);
		pagedir_destroy (pd);
	}

	if(lock_held_by_current_thread (&fevict))
		lock_release(&fevict);
}

/* Sets up the CPU for running user code in the current thread.
   This function is called on every context switch. */
	void
process_activate (void)
{
	struct thread *t = thread_current ();

	/* Activate thread's page tables. */
	pagedir_activate (t->pagedir);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update ();
}
/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
	unsigned char e_ident[16];
	Elf32_Half    e_type;
	Elf32_Half    e_machine;
	Elf32_Word    e_version;
	Elf32_Addr    e_entry;
	Elf32_Off     e_phoff;
	Elf32_Off     e_shoff;
	Elf32_Word    e_flags;
	Elf32_Half    e_ehsize;
	Elf32_Half    e_phentsize;
	Elf32_Half    e_phnum;
	Elf32_Half    e_shentsize;
	Elf32_Half    e_shnum;
	Elf32_Half    e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
	Elf32_Word p_type;
	Elf32_Off  p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
	bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
	struct thread *t = thread_current ();
	struct Elf32_Ehdr ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i = 0;

	char fname[MAX_NAME_LEN];
	const char* s_ptr = file_name;
	while(s_ptr[i] == ' ')
		++i;

	while(s_ptr[i] != ' ' && s_ptr[i] != '\0')
	{
		fname[i] = s_ptr[i];
		++i;
	}
	fname[i] = '\0';

	/* Allocate and activate page directory. */
	t->pagedir = pagedir_create ();
	if (t->pagedir == NULL) 
	{
		goto done;
	}
	process_activate ();

	/* Open executable file. */
	file = filesys_open (fname);
	if (file == NULL) 
	{
		printf ("load: %s: open failed\n", file_name);
		goto done; 
	}

	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 3
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
			|| ehdr.e_phnum > 1024) 
	{
		printf ("load: %s: error loading executable\n", file_name);
		goto done; 
	}

	hash_init (&t->spagedir, spage_hash_hash_func, spage_hash_less_func, NULL);

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) 
	{
		struct Elf32_Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) 
		{
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) 
				{
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint32_t file_page = phdr.p_offset & ~PGMASK;
					uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint32_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0)
					{
						/* Normal segment.
						   Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					}
					else 
					{
						/* Entirely zero.
						   Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack (esp))
		goto done;

	/* push all arguments onto the user stack. BDH */
	char* str_ptr = (char*) file_name;
	*esp = PHYS_BASE - 12;
	int num_bytes = count_bytes(&str_ptr);
	*esp = push_arguments(num_bytes, str_ptr, file_name);
	//hex_dump((uintptr_t)* esp, *esp, PHYS_BASE - *esp, true);

	/***************** ARGS PASSING TEST CODE *******************
	  char *file_test = "/bin/ls -l foo bar";
	  char *str_ptr = file_test;
	  int num_bytes = count_bytes(&str_ptr);
	  hex_dump((uintptr_t)* esp, *esp, 1, true);
	 *esp = push_arguments(num_bytes, str_ptr, file_test);
	 hex_dump((uintptr_t)* esp, *esp, PHYS_BASE - *esp, true);
	 ************************************************************/

	/* Start address. */
	*eip = (void (*) (void)) ehdr.e_entry;
	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	if(!success)
	{
		file_close (file);
		exec_load_status = false;
		sema_up(&exec_load_sema);
	}
	else
	{
		thread_current()->file = file;
		file_deny_write(thread_current()->file);
		exec_load_status = true;
		sema_up(&exec_load_sema);
	}
	return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
	static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
		return false; 

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (Elf32_Off) file_length (file)) 
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz) 
		return false; 

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

   - READ_BYTES bytes at UPAGE must be read from FILE
   starting at offset OFS.

   - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
	static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) 
	{
		/* Calculate how to fill this page.
		   We will read PAGE_READ_BYTES bytes from FILE
		   and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct spage * p = (struct spage *) malloc(sizeof(struct spage));
		if(p == NULL)
			return false;

		p->addr = upage;
		if(page_read_bytes == PGSIZE) // FILE
		{
			p->state = DISK;
		}
		else if(page_zero_bytes == PGSIZE) // ZERO
		{
			p->state = ZERO;
		}
		else // SWAP
		{
			p->state = MIXED;
		}
		p->file = file;
		p->ofs = ofs;
		p->readonly = writable;
		p->page_read_bytes = page_read_bytes;
		p->page_zero_bytes = page_zero_bytes;
		p->swapindex = BITMAP_ERROR;
		lock_init(&p->spagelock);
		hash_insert (&thread_current()->spagedir, &p->hash_elem);

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
	static bool
setup_stack (void **esp) 
{
	uint8_t *kpage;
	bool success = false;

	kpage = frame_selector (((uint8_t *) PHYS_BASE) - PGSIZE, PAL_USER | PAL_ZERO);
	if (kpage != NULL) 
	{
		stack_bound = ((uint8_t *) PHYS_BASE) - PGSIZE;
		success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
		if (success)
		{
			*esp = PHYS_BASE;
			// Add stack page to supplementary page table - rds
			struct spage * p = (struct spage *) malloc(sizeof(struct spage));
			if(p != NULL)
			{
				p->addr = ((uint8_t *) PHYS_BASE) - PGSIZE;
				p->state = ZERO;
				p->file = NULL;
				p->ofs = 0;
				p->readonly = true;
				p->page_read_bytes = 0;
				p->page_zero_bytes = PGSIZE;
				p->swapindex = BITMAP_ERROR;
				lock_init(&p->spagelock);
				hash_insert (&thread_current()->spagedir, &p->hash_elem);
			}
		}
		else
		{
			palloc_free_page (kpage);
		}
	} 

	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
	bool
install_page (void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	   address, then map our page there. */
	return (pagedir_get_page (t->pagedir, upage) == NULL
			&& pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/* Returns the number of bytes needed to hold a string vector with the space
   delimited segments of the string pointed to by *str_ptr. After returning, str_ptr
   will point to the final element of the string, which should be '\0'. BDH */
static int count_bytes(char **str_ptr) 
{
	int argc = 0;
	int num_bytes = 0;
	int in_word = 0;
	char c;

	while ((c = **str_ptr) != '\0') {
		if (c != ' ') {
			if (!in_word) {
				if(num_bytes < (MAX_NUM_BYTES - argc * 4))
					++num_bytes; // extra bytes for the \0
				in_word = 1;
			}
			if(num_bytes < (MAX_NUM_BYTES - argc * 4))
				++num_bytes;
		} else {
			in_word = 0;
		}

		++(*str_ptr);
	}

	return num_bytes;
}

/* pushes arguments onto the stack and returns. BDH */
char** push_arguments(int num_bytes, char *str_ptr, const char *base)
{
	int argc = 1;
	int in_word = 1;
	char c;

	char *stack_ptr = PHYS_BASE; // initialize stack pointer for pushing
	str_ptr++; // increment by one to use usual popping idiom

	/* rather than fooling with ugly and possibly dangerous casts, here we
	   can achieve alignment with a somewhat elegant mathematical mechanism.
	   since PHYS_BASE is guaranteed to be divisible by 4, we need only
	   ensure that num_bytes is as well before subtraction to achieve 
	   alignment */

	int mod = num_bytes & 3;
	num_bytes = (mod == 0 ? num_bytes : (num_bytes & ~3) + 4);

	// set up argv_ptr
	char **argv_ptr = PHYS_BASE - num_bytes; // start of strings 
	*--argv_ptr = NULL; // push NULL address to terminate argv

	int argsize = 0;
	const int maxsize = num_bytes;
	/* we're going to read the chars from str_ptr 1 at a time, writing them to
	   memory if they're not spaces and creating a new entry in argv if they
	   are. This also assumes the only whitespace we can get is spaces. We count
	   the number of arguments by counting the number of 'words' in the line */
	while (base < str_ptr && argsize < maxsize) {
		c = *--str_ptr;

		if (c != ' ') {

			if (!in_word) {
				argc++;
				in_word = 1;
				*--stack_ptr = '\0';
				++argsize;
			}
			if(argsize < maxsize)
			{
				*--stack_ptr = c;
				++argsize;
			}
		} else {
			if (in_word) {
				*--argv_ptr = stack_ptr; // push argv entry since no longer word
			}
			in_word = 0;
		}
	}

	/* handle an edge case of leading spaces */
	if (*str_ptr != ' ')
		*--argv_ptr = stack_ptr;

	// write argv base address, two steps to minimize confusion
	argv_ptr--;
	*argv_ptr = argv_ptr + 1;

	// just make a dummy int pointer to push argc
	int* int_ptr = (int*) argv_ptr;
	*--int_ptr = argc;

	// make argv_ptr the value of int_ptr to push the dummy return address
	argv_ptr = (char**) int_ptr;
	*--argv_ptr = NULL;
	return argv_ptr;
}

bool validCTID(tid_t child_tid)
{
	struct list_elem *e;
	if(thread_current()->numchild > 0)
	{
		for (e = list_begin (&thread_current()->child_list); e != list_end (&thread_current()->child_list); e = list_next (e))
		{
			struct childproc * c = list_entry (e, struct childproc, elem);
			if(c->childid == child_tid)
				return true;
		}
	}
	return false;
}

bool checkWaitList(tid_t child_tid)
{
	struct list_elem *e;
	if(thread_current()->numchild > 0)
	{
		for (e = list_begin (&thread_current()->wait_list); e != list_end (&thread_current()->wait_list); e = list_next (e))
		{
			struct childproc * c = list_entry (e, struct childproc, elem);
			if(c->childid == child_tid)
				return true;
		}
	}
	return false;
}

bool checkCTID(tid_t child_tid)
{
	struct exitstatus nes;
	nes.avail = false;
	nes.status = -1;
	nes.childid = child_tid;
	exit_foreach(getExitStatus, (void*) &nes);
	return nes.avail;
}

int getCTID(tid_t child_tid)
{
	struct exitstatus nes;
	nes.avail = false;
	nes.status = -1;
	nes.childid = child_tid;
	exit_foreach(getExitStatus, (void*) &nes);
	return nes.status;
}

void getExitStatus(struct exitstatus * es, void* aux)
{
	struct exitstatus * nes = (struct exitstatus *) aux;
	if(es->childid == nes->childid)
	{
		nes->avail = es->avail;
		nes->status = es->status;
	}
}

void addChildProc(tid_t childid)
{
	if(thread_current()->numchild == 0)
	{
		list_init(&thread_current()->child_list);
		list_init(&thread_current()->wait_list);
	}

	struct childproc * child = (struct childproc *) malloc(sizeof(struct childproc));
	if(child != NULL)
	{
		child->childid = childid;
		list_push_back(&thread_current()->child_list, &child->elem);
		++thread_current()->numchild;
	}
}

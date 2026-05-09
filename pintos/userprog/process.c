#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/cstr.h"
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);
static bool duplicate_fd_table (struct thread *curr, struct thread *parent);
static bool init_fd_table (struct thread *t);

struct fork_args {
	struct thread *parent;
	struct intr_frame if_;
	struct child_status *cs;
};

struct initd_args {
	char *file_name;
	struct child_status *cs;
};

#define FD_MAX (PGSIZE / sizeof (struct file *))

/* Common process initialization hook. */
static void
process_init (void) {
	struct thread *current UNUSED = thread_current ();
}

tid_t
process_create_initd (const char *file_name) {
	struct thread *curr = thread_current ();
	char *file_name_copy;
	char program_name[THREAD_NAME_MAX];
	char *arg_start;
	struct child_status *cs = NULL;
	struct initd_args *args = NULL;
	tid_t tid = TID_ERROR;

	if (curr == NULL)
		return TID_ERROR;

	file_name_copy = palloc_get_page (0);
	if (file_name_copy == NULL)
		return TID_ERROR;
	strlcpy (file_name_copy, file_name, PGSIZE);

	strlcpy (program_name, file_name, sizeof program_name);
	arg_start = strchr (program_name, ' ');
	if (arg_start != NULL)
		*arg_start = '\0';

	cs = malloc (sizeof *cs);
	if (cs == NULL)
		goto done;

	cs->tid = TID_ERROR;
	cs->exit_status = -1;
	cs->waited = false;
	cs->exited = false;
	cs->fork_success = false;
	sema_init (&cs->fork_sema, 0);
	sema_init (&cs->wait_sema, 0);

	list_push_back (&curr->child_status_list, &cs->elem);

	args = malloc (sizeof *args);
	if (args == NULL)
		goto done;

	args->file_name = file_name_copy;
	args->cs = cs;

	tid = thread_create (program_name, PRI_DEFAULT, initd, args);
	if (tid == TID_ERROR)
		goto done;

	cs->tid = tid;
	return tid;
done:
	if (args != NULL)
		free (args);
	if (tid == TID_ERROR)
		palloc_free_page (file_name_copy);
	if (cs != NULL) {
		list_remove (&cs->elem);
		free (cs);
	}
	return TID_ERROR;
}

/* Starts the first user process. */
static void
initd (void *f_name) {
	struct initd_args *args = f_name;
	struct thread *curr = thread_current ();

	if (curr == NULL || args == NULL)
		thread_exit ();

#ifdef VM
	supplemental_page_table_init (&curr->spt);
#endif

	curr->self_status = args->cs;
	if (!init_fd_table (curr)) {
		thread_exit ();
	}

	process_init ();
	f_name = args->file_name;
	free (args);

	if (process_exec (f_name) < 0)
		thread_exit ();
	NOT_REACHED ();
}
/* Forks the current process as NAME. */
tid_t
process_fork (const char *name, struct intr_frame *if_) {
	struct thread *curr = thread_current ();
	struct child_status *cs = NULL;
	struct fork_args *args = NULL;
	tid_t tid = TID_ERROR;

	if (curr == NULL)
		return TID_ERROR;

	cs = malloc (sizeof *cs);
	if (cs == NULL)
		goto done;

	cs->tid = TID_ERROR;
	cs->exit_status = -1;
	cs->waited = false;
	cs->exited = false;
	cs->fork_success = false;
	sema_init (&cs->fork_sema, 0);
	sema_init (&cs->wait_sema, 0);

	list_push_back (&curr->child_status_list, &cs->elem);

	args = malloc (sizeof *args);
	if (args == NULL)
		goto done;

	args->parent = curr;
	args->if_ = *if_;
	args->cs = cs;

	tid = thread_create (name, PRI_DEFAULT, __do_fork, args);
	if (tid == TID_ERROR)
		goto done;

	args = NULL;
	cs->tid = tid;
	sema_down (&cs->fork_sema);
	if (!cs->fork_success) {
		tid = TID_ERROR;
		goto done;
	}

	return tid;

done:
	if (args != NULL)
		free (args);
	if (cs != NULL) {
		list_remove (&cs->elem);
		free (cs);
	}
	return TID_ERROR;
}

#ifndef VM
/* pml4_for_each callback used by the non-VM fork path. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	if (is_kernel_vaddr (va))
		return true;

	parent_page = pml4_get_page (parent->pml4, va);
	if (parent_page == NULL)
		return false;

	newpage = palloc_get_page (PAL_USER);
	if (newpage == NULL)
		return false;

	memcpy (newpage, parent_page, PGSIZE);
	writable = is_writable (pte);

	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		palloc_free_page (newpage);
		return false;
	}
	return true;
}
#endif

static bool
duplicate_fd_table (struct thread *curr, struct thread *parent) {
	int fd;

	if (curr == NULL || parent == NULL) {
		return false;
	}

	if (!init_fd_table (curr)) {
		return false;
	}

	if (parent->fd_table == NULL) {
		curr->next_fd = 2;
		return true;
	}

	for (fd = 2; fd < parent->next_fd && fd < FD_MAX; fd++) {
		if (parent->fd_table[fd] == NULL)
			continue;

		curr->fd_table[fd] = file_duplicate (parent->fd_table[fd]);
		if (curr->fd_table[fd] == NULL)
			goto error;
	}

	curr->next_fd = parent->next_fd;
	return true;

error:
	for (fd = 2; fd < FD_MAX; fd++) {
		if (curr->fd_table[fd] == NULL)
			continue;
		file_close (curr->fd_table[fd]);
		curr->fd_table[fd] = NULL;
	}
	palloc_free_page (curr->fd_table);
	curr->fd_table = NULL;
	curr->next_fd = 2;
	return false;
}

static bool
init_fd_table (struct thread *t) {
	if (t == NULL)
		return false;
	if (t->fd_table != NULL)
		return true;

	t->fd_table = palloc_get_page (PAL_ZERO);
	if (t->fd_table == NULL)
		return false;

	t->next_fd = 2;
	return true;
}

/* Child thread entry point for fork(). */
static void
__do_fork (void *aux) {
	struct fork_args *args = aux;
	struct thread *curr = thread_current ();
	struct thread *parent;
	struct intr_frame if_;

	if (curr == NULL || args == NULL)
		thread_exit ();

	parent = args->parent;
	if (parent == NULL) {
		free (args);
		thread_exit ();
	}

	curr->self_status = args->cs;
	memcpy (&if_, &args->if_, sizeof if_);
	free (args);

	curr->pml4 = pml4_create ();
	if (curr->pml4 == NULL)
		goto error;

	process_activate (curr);
#ifdef VM
	supplemental_page_table_init (&curr->spt);
	if (!supplemental_page_table_copy (&curr->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif
	ASSERT (curr->fd_table == NULL);
	if (!duplicate_fd_table (curr, parent))
		goto error;

	if_.R.rax = 0;

	process_init ();
	curr->self_status->fork_success = true;
	sema_up (&curr->self_status->fork_sema);

	do_iret (&if_);
error:
	if (curr->self_status != NULL) {
		struct child_status *cs = curr->self_status;

		cs->exit_status = -1;
		cs->exited = true;
		cs->fork_success = false;
		curr->self_status = NULL;
		sema_up (&cs->fork_sema);
	}
	thread_exit ();
}

/* Replaces the current process image with F_NAME. */
int
process_exec (void *f_name) {
	char *file_name = f_name;
	bool success;
	struct thread *curr = thread_current ();

	if (curr == NULL)
		return -1;

	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	process_cleanup ();

	ASSERT (curr->fd_table != NULL);

	success = load (file_name, &_if);
	palloc_free_page (file_name);
	if (!success)
		return -1;

	do_iret (&_if);
	NOT_REACHED ();
}

/* Waits for CHILD_TID and returns its exit status. */
int
process_wait (tid_t child_tid) {
	struct thread *curr = thread_current ();
	int status;

	if (curr == NULL)
		return -1;

	struct list *childs = &curr->child_status_list;
	struct list_elem *e;
	struct child_status *cs;
	for (e = list_begin (childs); e != list_end (childs); e = list_next (e)) {
		cs = list_entry (e, struct child_status, elem);

		if (cs->tid != child_tid)
			continue;

		if (cs->waited)
			return -1;

		cs->waited = true;

		if (!cs->exited)
			sema_down (&cs->wait_sema);

		status = cs->exit_status;
		list_remove (&cs->elem);
		free (cs);
		return status;
	}
	return -1;
}

/* Called from thread_exit() when the current process terminates. */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	if (curr == NULL)
		return;

	int fd;

	for (fd = 2; fd < FD_MAX; fd++)
		process_close_file (fd);

	if (curr->fd_table != NULL) {
		palloc_free_page (curr->fd_table);
		curr->fd_table = NULL;
	}

	if (curr->self_status != NULL && !curr->self_status->exited) {
		curr->self_status->exited = true;
		sema_up (&curr->self_status->wait_sema);
	}

	process_cleanup ();
}

int
process_add_file (struct file *f) {
	struct thread *curr = thread_current ();
	int fd;

	if (curr == NULL || f == NULL || curr->fd_table == NULL)
		return -1;

	for (fd = 2; fd < FD_MAX; fd++) {
		if (curr->fd_table[fd] != NULL)
			continue;

		curr->fd_table[fd] = f;

		if (fd >= curr->next_fd)
			curr->next_fd = fd + 1;
		return fd;
	}
	return -1;
}

struct file *
process_get_file (int fd) {
	struct thread *curr = thread_current ();

	if (curr == NULL || fd < 0 || fd >= FD_MAX || curr->fd_table == NULL)
		return NULL;

	return curr->fd_table[fd];
}

void
process_close_file (int fd) {
	struct thread *curr = thread_current ();

	if (curr == NULL || fd < 2 || fd >= FD_MAX || curr->fd_table == NULL)
		return;

	if (curr->fd_table[fd] == NULL)
		return;

	file_close (curr->fd_table[fd]);
	curr->fd_table[fd] = NULL;
	if (fd < curr->next_fd)
		curr->next_fd = fd;
}

/* Releases the current process's address-space resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Activates NEXT's user address space. */
void
process_activate (struct thread *next) {
	pml4_activate (next->pml4);
	tss_update (next);
}

/* ELF constants and headers. */
#define EI_NIDENT 16

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_STACK 0x6474e551

#define PF_X 1
#define PF_W 2
#define PF_R 4

struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

#define ELF  ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads FILE_NAME into the current thread. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;
	char *argv[64];
	char *token, *save_point;
	char *fn_copy = NULL;
	int argc = 0;

	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (t);

	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return false;
	memcpy (fn_copy, file_name, CSTR_SIZE (file_name));

	token = strtok_r (fn_copy, " ", &save_point);
	while (token != NULL) {
		if ((size_t) argc >= sizeof argv / sizeof *argv)
			goto done;
		argv[argc++] = token;
		token = strtok_r (NULL, " ", &save_point);
	}

	if (argc == 0)
		goto done;

	file = filesys_open (argv[0]);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr ||
	    memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7) || ehdr.e_type != 2 ||
	    ehdr.e_machine != 0x3E || ehdr.e_version != 1 ||
	    ehdr.e_phentsize != sizeof (struct Phdr) || ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;

		switch (phdr.p_type) {
		case PT_NULL:
		case PT_NOTE:
		case PT_PHDR:
		case PT_STACK:
		default:
			break;
		case PT_DYNAMIC:
		case PT_INTERP:
		case PT_SHLIB:
			goto done;
		case PT_LOAD:
			if (validate_segment (&phdr, file)) {
				bool writable = (phdr.p_flags & PF_W) != 0;
				uint64_t file_page = phdr.p_offset & ~PGMASK;
				uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
				uint64_t page_offset = phdr.p_vaddr & PGMASK;
				uint32_t read_bytes, zero_bytes;

				if (phdr.p_filesz > 0) {
					read_bytes = page_offset + phdr.p_filesz;
					zero_bytes =
					        (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE) -
					         read_bytes);
				} else {
					read_bytes = 0;
					zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
				}

				if (!load_segment (file, file_page, (void *) mem_page,
				                   read_bytes, zero_bytes, writable))
					goto done;
			} else
				goto done;
			break;
		}
	}

	if (!setup_stack (if_))
		goto done;

	char *stack_p = (char *) if_->rsp;

	if_->rip = ehdr.e_entry;

	/* Copy argument strings and argv pointers onto the user stack. */
	for (int argi = argc - 1; argi >= 0; argi--) {
		int size = CSTR_SIZE (argv[argi]);
		stack_p -= size;
		memcpy (stack_p, argv[argi], size);
		argv[argi] = stack_p;
	}

	stack_p = (char *) ((uintptr_t) stack_p & -8);

	stack_p -= 8;
	memset (stack_p, 0, 8);

	for (int n = argc - 1; n >= 0; n--) {
		stack_p -= 8;
		*(uintptr_t *) stack_p = (uintptr_t) argv[n];
	}

	if_->R.rsi = (uint64_t) stack_p;
	if_->R.rdi = argc;

	stack_p -= 8;
	memset (stack_p, 0, 8);
	if_->rsp = (uintptr_t) stack_p;

	// hex_dump(if_->rsp, if_->rsp, USER_STACK - (uint64_t)if_->rsp, true);
	success = true;

done:
	if (fn_copy != NULL)
		palloc_free_page (fn_copy);

	if (file != NULL)
		file_close (file);
	return success;
}

#ifndef VM
/* Non-VM loader path. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Immediately loads a segment into physical pages. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		if (file_read (file, kpage, page_read_bytes) !=
		    (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		if (!install_page (upage, kpage, writable)) {
			printf ("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Maps the initial stack page. */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success =
		        install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Maps user virtual page UPAGE to kernel page KPAGE. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	return (pml4_get_page (t->pml4, upage) == NULL &&
	        pml4_set_page (t->pml4, upage, kpage, writable));
}


#else
/* VM loader path. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Read this page's segment contents on first fault. */
}

/* Registers lazy pages for a loadable segment. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Pass file offset/read/zero metadata through aux. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage, writable,
		                                     lazy_load_segment, aux))
			return false;

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Creates and claims the initial stack page. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Allocate, claim, and mark stack_bottom as a stack page. */

	return success;
}
#endif /* VM */

/* Returns whether PHDR describes a valid loadable segment. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	if (phdr->p_vaddr < PGSIZE)
		return false;

	return true;
}

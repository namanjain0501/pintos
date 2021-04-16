#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

struct file_details{
	int fd;
	struct file *file_ptr;
	struct list_elem file_elem;
};

struct semaphore sem;

// handling functions declaration
int read_handle(int fd, void *buffer, unsigned size);
int write_handle(int fd, void *buffer, unsigned size);
void exit_handle(int status);
pid_t exec_handle(const char *cmd_line);
int wait_handle(pid_t pid);
bool create_handle(const char *file, unsigned inital_size);
bool remove_handle(const char *file);
int open_handle(const char *file);
int filesize_handle(int fd);
void close_handle(int fd);

// searching file ptr using fd in open_files list
struct file *get_fileptr_from_fd(int fd)
{
	struct thread *cur = thread_current();
	struct list_elem *it;
	for(it = list_begin(&cur->open_files); it!= list_end(&cur->open_files); it=list_next(it))
	{
		struct file_details *f = list_entry(it, struct file_details, file_elem);
		if(f->fd == fd)
			return f->file_ptr;
	} 

	return NULL;
}


static void syscall_handler (struct intr_frame *);

// check if vaddr is a valid in user virtual address
void check_valid_pointer(void *vaddr)
{
	if(!is_user_vaddr(vaddr) || vaddr < 0x08048000 )
		return exit_handle(-1);
}

// check if arguments are 
void check_arguments(uint32_t *args,int n)
{
	int i;
	for(i=0;i<n;i++)
	{
		check_valid_pointer((void *)(args+i));
	}
}

// check for valid string in user virtual memory
void check_string(void *s, int size)
{
	int i;
	for(i=0;i<size;i++)
		check_valid_pointer(s+i);
}

int
get_page(const void *vaddr)
{
  void *ptr = pagedir_get_page(thread_current()->pagedir,vaddr);
  if (!ptr)
  {
    exit_handle(-1);
  }
  else
 	 return (int)ptr;
}

// for unknown size
void check_string2(void *s)
{
	while(1)
	{
		if(*(char *)get_page(s) == 0)
			break;
		s = (char *)s+1;
	}
}

void
syscall_init (void) 
{
	// initializing semaphore
	sema_init(&sem,1);

  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	uint32_t *args = f->esp;

  if((int)args[0] == SYS_READ)
  {
  	check_arguments(args+1, 3);
  	check_string((void *)args[6],(int)args[7]);
  	f->eax = read_handle((int)args[5], (void *)args[6], (unsigned)args[7]);
  }
  else if((int)args[0] == SYS_WRITE)
  {
  	check_arguments(args+5,3);
  	check_string((void *)args[6],(int)args[7]);

  	f->eax = write_handle((int)(args[5]),(char *)args[6],(unsigned)args[7]);
  }
  else if((int)args[0] == SYS_EXIT)
  {
  	check_arguments(args+1,1);
  	
  	exit_handle(*((int *)((int *)f->esp+1)));
  }
  else if((int)args[0] == SYS_EXEC)
  {
  	check_arguments(args+1,1);
  	check_string2(args[1]);
  	f->eax = exec_handle((const char *)args[1]);
  }
  else if((int)args[0] == SYS_WAIT)
  {
  	check_arguments(args+1,1);
  	f->eax = wait_handle((pid_t)args[1]);
  }
  else if((int)args[0] == SYS_CREATE)
  {
  	check_arguments(args+4,2);
  	check_string2(args[4]);
  	f->eax = create_handle((const char *)args[4], (unsigned)args[5]);
  }
  else if((int)args[0] == SYS_REMOVE)
  {
  	check_arguments(args+1,1);
  	check_string2(args[1]);
  	f->eax = remove_handle((const char *)args[1]);
  }
  else if((int)args[0] == SYS_OPEN)
  {
  	check_arguments(args+1,1);
  	check_string2(args[1]);
  	f->eax = open_handle((const char *)args[1]);
  }
  else if((int)args[0] == SYS_FILESIZE)
  {
  	check_arguments(args+1,1);
  	f->eax = filesize_handle((int)args[1]);
  }
  else if((int)args[0] == SYS_CLOSE)
  {
  	check_arguments(args+1,1);
  	close_handle((int)args[1]);
  }

}

int read_handle(int fd, void *buffer, unsigned size)
{
	int ans=-1;

	get_page(buffer);
	get_page(buffer+size-1);

	if(fd == 0) // stdin
	{
		int i;
		for(i=0;i<size;i++)
		{
			*((char *)buffer+i) = (char)input_getc();
		}
		ans=size;
	}
	else
	{
		struct file *f = get_fileptr_from_fd(fd);
		// if file not found!
		if(f == NULL)
			return -1; 

		ans = file_read(f, buffer, size);
	}

	return ans;
}

int write_handle(int fd, void *buffer, unsigned size)
{
	get_page(buffer);
	get_page(buffer+size-1);

	// lock
	sema_down(&sem);

	int ans=-1;

	if(fd == 1) //stdout
	{
		putbuf((char *)buffer, size);
		ans=size;
	}
	else
	{

		struct file *f = get_fileptr_from_fd(fd);
		// if file not found!
		if(f == NULL)
		{
			sema_up(&sem);
			return -1;
		}

		ans = file_write(f, buffer, size);
	}

	// release lock
	sema_up(&sem);
	return ans;
}

void exit_handle(int status)
{
	struct thread *t;
	t = thread_current ();
	// t->status = status;

	if(t->parent_thread)
	{
		t->parent_thread->child_exit_status = status;
	}
	
	// close open files,., tobe done
	printf ("%s: exit(%d)\n", t->name, status);

	thread_exit ();

}

pid_t exec_handle(const char *cmd_line)
{
	pid_t pid = process_execute(cmd_line);
	struct thread *child = get_thread_pointer(pid);
	if(child == NULL)
		return -1;

	// wait for loading
	sema_down(&(child->load_sem));


	if(child->load_status == -1) // not loaded successfully
		return -1;
	else
		return pid;
}

int wait_handle(pid_t pid)
{
	int a = process_wait(pid);
	return a;
}

bool create_handle(const char *file, unsigned initial_size)
{
	//lock
	sema_down(&sem);

	bool status = filesys_create(file, initial_size);

	//unlock
	sema_up(&sem);

	return status;
}

bool remove_handle(const char *file)
{
	//lock
	sema_down(&sem);

	bool status = filesys_remove(file);

	//unlock
	sema_up(&sem);

	return status;
}

int open_handle(const char *file)
{
	//lock
	sema_down(&sem);

	struct file *f = filesys_open(file);


	// if file failed to open
	if(f == NULL)
	{
		sema_up(&sem);
		return -1;
	}

	struct file_details *file_det;
	file_det = malloc(sizeof(struct file_details));
	file_det->fd = thread_current()->cur_fd++;
	file_det->file_ptr = f;

	// insert file to open_files list
	list_push_back(&thread_current()->open_files, &(file_det->file_elem));

	//unlock
	sema_up(&sem);

	return file_det->fd;
}

int filesize_handle(int fd)
{
	//lock
	sema_down(&sem);

	struct file *file_ptr = get_fileptr_from_fd(fd);

	// if file not found
	if(file_ptr == NULL)
	{
		sema_up(&sem);
		return -1;
	}

	int sz = file_length(file_ptr);

	//unlock
	sema_up(&sem);

	return sz;
}

// iterate through open_files list and remove file with file descriptor=fd
// fd=-1 means close all opened files
void close_handle(int fd)
{
	//lock
	sema_down(&sem);

	struct thread *cur = thread_current();
	struct list_elem *it,*next;
	for(it = list_begin(&cur->open_files); it!= list_end(&cur->open_files); it=next)
	{
		next = list_next(it);
		struct file_details *f = list_entry(it, struct file_details, file_elem);
		// remove file if it is the required file
		if(f->fd == fd || fd == -1)
		{
			file_close(f->file_ptr);
			list_remove(&f->file_elem);
			free(f);
		}
	}

	//unlock
	sema_up(&sem);
}
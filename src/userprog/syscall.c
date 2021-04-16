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

struct semaphore sem;

static void syscall_handler (struct intr_frame *);

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
  // printf ("system call!\n");

  if(*((int *)(f->esp)) == SYS_READ)
  {
  	f->eax = read_handle(*((int *)((int *)f->esp+1)), (void *)((int *)f->esp+2), (unsigned)(*((int *)f->esp+3)));
  }
  else if(*((int *)(f->esp)) == SYS_WRITE)
  {
  	uint32_t *args = f->esp;
  	// hex_dump(0,(void *)((int *)f->esp),1000,true);

  	// printf("fd: %d\n",(int)(args[5]));
  	// printf("buffer: %s\n",(char *)args[6]);
  	// printf("size: %d\n",(unsigned)args[7]);

  	f->eax = write_handle((int)(args[5]),(char *)args[6],(unsigned)args[7]);
  }
  else if(*((int *)(f->esp)) == SYS_EXIT)
  {
  	// printf("exit system call\n");
  	
  	f->eax = exit_handle(*((int *)((int *)f->esp+1)));
  }
  else
  {
  	printf(" unknown system call\n");
  }

}

int read_handle(int fd, void *buffer, unsigned size)
{
	// lock
	sema_down(&sem);

	int ans=-1;

	//checks
	if(!is_user_vaddr(buffer) || !is_user_vaddr(buffer+size-1))
	{
		// release lock
		sema_up(&sem);
		// invalid.. exit
		exit_handle(-1);
	}

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
		// read from file.. to be done
	}

	// release lock 
	sema_up(&sem);
	return ans;
}

int write_handle(int fd, void *buffer, unsigned size)
{
	// lock
	sema_down(&sem);

	int ans=-1;

	//checks
	if(!is_user_vaddr(buffer) || !is_user_vaddr(buffer+size-1))
	{
		// release lock
		sema_up(&sem);
		// invalid.. exit
		exit_handle(-1);
	}

	if(fd == 1) //stdout
	{
		putbuf((char *)buffer, size);
		// printf("send to output buffer successfull\n");
		ans=size;
	}
	else
	{
		// write to file.. to be done
	}

	// release lock
	sema_up(&sem);
	return ans;
}

int exit_handle(int status)
{
	struct thread *t;
	t = thread_current ();

	// close open files,., tobe done
	printf ("%s: exit(%d)\n", t->name, status);

	thread_exit ();
}

#include <stdlib.h>
#include <stdio.h>
#include "iferret_syscall_stack.h"


iferret_syscall_stack_t *iferret_syscall_stack=NULL;

const iferret_syscall_stack_element_t not_found_element = {
    .syscall = {
        .eax = -1,
        .ebx = -1,
        .op_num = -1,
        .is_sysenter = -1,  // Distinguish between INT and sysenter methods
        .is_enter = -1,     // Is this a system call enter or exit?
        .pid = -1,
        .callsite_eip = -1,
        .command = NULL
    },
    .index = -1
};

static inline void *my_malloc(size_t n) {
  void *p;
  p = malloc(n);
  assert(p!=NULL);
  return (p);
}


static inline void *my_realloc(void *p, size_t n) {
  void *new_p;
  new_p = realloc(p,n);
  assert(new_p!=NULL);
  return (new_p);
}


void iferret_syscall_print(iferret_syscall_t syscall) {
  printf ("syscall(eax=%d,op_num=%d(%s),is_sysenter=%d,pid=%d,callsite_eip=%x)",
	  syscall.eax,
	  syscall.op_num,
	  iferret_op_num_to_str(syscall.op_num),
	  syscall.is_sysenter,
	  syscall.pid,
	  syscall.callsite_eip);
}


// initialize the per-pid syscall stacks. 
void iferret_syscall_stacks_init(){
  int i;
  if (iferret_syscall_stack == NULL) {
    iferret_syscall_stack = (iferret_syscall_stack_t *)
      my_malloc(sizeof(iferret_syscall_stack_t) * MAX_PID);
    for (i=0; i<MAX_PID; i++){
      iferret_syscall_stack[i].size = 0;
      iferret_syscall_stack[i].capacity = 0;
      iferret_syscall_stack[i].stack = NULL;
    }
  }
}


// check if table for pid is big enough.  grow if necessary
void _resize_syscall_stack(int pid) {
  iferret_syscall_stack_t *stack;
  // make sure they exist.
  iferret_syscall_stacks_init();
  // local pointer 
  stack = &(iferret_syscall_stack[pid]);
  // no space at all
  if (stack->capacity == 0) {
    stack->capacity = 2;
    stack->stack = my_malloc(sizeof(iferret_syscall_stack_element_t) * stack->capacity);
  }
  else {
    // adding this element would overflow -- double capacity
    if (stack->size == stack->capacity) {      
      stack->capacity *= 2;
      stack->stack = my_realloc(stack->stack,
				sizeof(iferret_syscall_stack_element_t) * stack->capacity);
    }
  }
}


// die if pid not in allowed range
void _check_pid(int pid) {
  if ((pid < 0) || (pid > MAX_PID-1)) {
    printf("Error pid %d out of range\n", pid);
    exit(1);
  }	
}


// note we don't free anything.  We just forget what's there. 
void iferret_syscall_stack_kill_process(int pid) {
  iferret_syscall_stack[pid].size = 0;
}

void iferret_syscall_stack_kill_all_processes() {
  int i;
  for (i=0; i<MAX_PID; i++){
    iferret_syscall_stack[i].size = 0;
  }
}


// push this (eip,syscall_num) pair to stack for pid.
void iferret_syscall_stack_push(iferret_syscall_t syscall) {
  iferret_syscall_stack_t *stack;
  _check_pid(syscall.pid);
  // check to see if there's capacity in the stack  
  _resize_syscall_stack(syscall.pid);
  // this is the stack for this pid. 
  stack = &(iferret_syscall_stack[syscall.pid]);
  // add the element.
  // Make sure to do a deep copy, including string
  stack->stack[stack->size].syscall = syscall;
  stack->stack[stack->size].syscall.command = strdup(syscall.command);
  stack->stack[stack->size].index = stack->size; // element needs to know its own index as it will get disconnected
  stack->size++;
}


// just returns the number of elements in stack for pid
//int iferret_syscall_stack_get_size(int pid) {
//  iferret_syscall_stacks_init();
//  return iferret_syscall_stack[pid].size;
//}


// deletes the element at this index within the stack for pid.
void iferret_syscall_stack_delete_at_index(int pid, int index) {
  iferret_syscall_stack_t *stack;
  iferret_syscall_stack_element_t to_delete;
  int i;	
  _check_pid(pid);
  iferret_syscall_stacks_init();
  stack = &(iferret_syscall_stack[pid]);
  if (stack->size == 0) {
    printf("Error, attempting to underflow\n");
    exit(1);
  }

  // Free the command string
  to_delete = iferret_syscall_stack_get_at_index(pid, index);
  free(to_delete.syscall.command);

  if (stack->size == 1) {
    // only one there -- just decrease stack size by one.
  }
  else {
    // move everything down by one to delete element at index  
    // note that offset=0 means delete element from top of stack,
    // offset=1 means delete element one down from top, etc.
    //    for(i=stack->size - offset - 1; i < stack->size - 1; i++) {
    for (i=index; i<stack->size-1; i++) {
      stack->stack[i].syscall = stack->stack[i+1].syscall;
      stack->stack[i].index = i;
    }
  }
  stack->size --;
}

void iferret_syscall_stack_delete_at_offset(int pid, int offset) {
  iferret_syscall_stack_t *stack;
  _check_pid(pid);
  iferret_syscall_stacks_init();
  stack = &(iferret_syscall_stack[pid]);
  iferret_syscall_stack_delete_at_index(pid, stack->size - 1 - offset);
}


// Return the element at offset for pid's stack, or a default value if
// there stack isn't deep enough to have that value, to indicate not found.
iferret_syscall_stack_element_t iferret_syscall_stack_get_at_index(int pid, int index) {
  iferret_syscall_stack_t *stack;
  _check_pid(pid);
  iferret_syscall_stacks_init();
  stack = &(iferret_syscall_stack[pid]);
  if (index<0 || index>=stack->size) {
    return not_found_element;		
  }
  return stack->stack[index];
}


// same as ..._index except offset is index wrt to end, i.e. top of stack
// that is, offset=0 is last element or stack->size-1.
// offset=1 is 2nd to last, i.e. stack->size-2.
// etc.
iferret_syscall_stack_element_t iferret_syscall_stack_get_at_offset(int pid, int offset) {
  iferret_syscall_stack_t *stack;
  _check_pid(pid);
  iferret_syscall_stacks_init();
  stack = &(iferret_syscall_stack[pid]);
  assert (offset >= 0);
  return (iferret_syscall_stack_get_at_index(pid, stack->size - 1 - offset));
}


// search the stack (start with most recently added element) 
// for this pid linearly until you find first item matching eip. 
// Return that element. 
// NB: special element with .eip slot set to -1 will be returned if not found. 
iferret_syscall_stack_element_t iferret_syscall_stack_get_with_eip(int pid, int this_eip, int another_eip) {
  int index;
  iferret_syscall_stack_element_t element;
  iferret_syscall_stack_t *stack;
  _check_pid(pid);
  iferret_syscall_stacks_init();
  stack = &(iferret_syscall_stack[pid]);

  if (stack->size == 0) {
    return not_found_element;
  }

  for (index=stack->size-1; index>=0; index--) {
    element = iferret_syscall_stack_get_at_index(pid,index);
    if (element.syscall.callsite_eip == this_eip) {
      // eips match 
      return element;
    }
    if ((another_eip != -1) && 
	(element.syscall.callsite_eip == another_eip)) {
	  // for some reason there is another way to match?
      return element;
    }
  }
  return not_found_element;
}



void iferret_syscall_stacks_print(){
  int pid,i,n,nn;
  
  if (iferret_syscall_stack != NULL) {
    printf ("syscall stack:\n");
    n=nn=0;
    for (pid=0; pid<MAX_PID; pid++) {
      iferret_syscall_stack_t *stack;
      stack = &(iferret_syscall_stack[pid]);
      if (stack!=NULL && stack->size > 0) {
	n++;
	printf ("pid=%d size=%d\n", pid, stack->size);
	for (i=0; i<stack->size; i++) {
	  printf ("  %d: ", i);
	  iferret_syscall_print(stack->stack[i].syscall);
	  printf ("\n");
	}
	nn += stack->size;
      }
    }
    printf ("%d pids with non-empty stacks.  %d elements in all stacks\n", n, nn);
  }
}
	     

void iferret_syscall_stacks_stats_print(){
  int pid,n,nn=0;

  if (iferret_syscall_stack != NULL) {
    n=nn=0;
    for (pid=0; pid<MAX_PID; pid++) {
      iferret_syscall_stack_t *stack;
      stack = &(iferret_syscall_stack[pid]);
      if (stack!=NULL && stack->size > 0) {
	n++;
	nn += stack->size;
	printf ("stack for pid=%d is %d\n", pid, stack->size);
      }
    }
    if (n==0) {
      printf ("\nsyscall_stack is empty\n\n");
    }
    else {
      printf ("\n%d pids with non-empty stacks.  %d elements in all stacks.  %.2f avg\n\n",
	      n, nn, ((float) nn) / ((float) n));
    }
  }
}


#if 0
void _add_syscall(int pid, int call_num, int callsite_eip) {
  iferret_syscall_t syscall;
  syscall.pid = pid;
  syscall.eax = call_num;
  syscall.callsite_eip = callsite_eip;
  iferret_syscall_stack_push(syscall);
}


void _del_syscall(int pid, int offset) {
  iferret_syscall_stack_delete_at_offset(pid,offset);
}


int main (int argc, char** argv) {
  iferret_syscall_stacks_init();

  _add_syscall(13,4,100);
  _del_syscall(13,0);
  _add_syscall(133,10,100);
  _del_syscall(133,0);
  // this fails -- nothing there for 134; attempted underflow
  // _del_syscall(134,12);

  _add_syscall(144,15,100);
  _add_syscall(32455,255,100);
  // another one for 144
  _add_syscall(144,133,200);

  printf("syscall for 144 @ 0: ");
  _print_syscall((iferret_syscall_stack_get_at_offset(144,0)).syscall);
  printf ("\n");

  printf("syscall for 144 @ 1: ");
  _print_syscall((iferret_syscall_stack_get_at_offset(144,1)).syscall);
  printf ("\n");
  
  printf("syscall for 144 with eip=100: ");
  _print_syscall((iferret_syscall_stack_get_with_eip(144,100,-1)).syscall);
  printf ("\n");
  
  _del_syscall(32455,0);  
  _del_syscall(144,0);
  _del_syscall(144,0);
  
 
  {
    int i;
    int f=100;
    for (i=0; i<1000000; i++) {
      printf ("i=%d\n", i);
      if ((i%1000) == 0) {
	printf ("\n i=%d f=%d\n", i,f);
	_print_stacks();
	f--;
      }

      if ((rand() % 100) < f) {
	int pid, eax, eip;
	pid = rand() % MAX_PID;
	eax = rand() % 500;
	eip = rand();	
	printf ("adding pid=%d eax=%d eip=%d\n", pid, eax, eip); 
	_add_syscall(pid,eax,eip);
      }
      else {
	int pid;
	pid = rand() % MAX_PID;
	if ((rand () % 2) == 0) {
	  int offset; 
	  iferret_syscall_stack_element_t element;
	  offset = rand() % 10;
	  element = iferret_syscall_stack_get_at_offset(pid,offset);
	  if (element.index != -1) {
	    printf ("deleting pid=%d offset=%d index=%d\n", pid, offset, element.index);
	    iferret_syscall_stack_delete_at_offset(pid,offset);
	  }
	}
	else { 
	  int eip;
	  eip = rand();
	  iferret_syscall_stack_element_t element;
	  element = iferret_syscall_stack_get_with_eip(pid,eip,-1);
	  if (element.index != -1) {
	    printf ("deleting pid=%d index=%d\n", pid, element.index);	    
	    iferret_syscall_stack_delete_at_index(pid,element.index);
	  }
	}
      }
    }
  }
  
  /*
  _add_syscall(14155,223,100);
  _add_syscall(1555,16,100);
  _add_syscall(32000,0,100);
  _add_syscall(1,35,100);
  _add_syscall(0,4,100);

  _del_syscall(14155,0);
  _del_syscall(1555,0);
  _del_syscall(1,0);
  */

  //  print_iferret_syscall_stack  ();
}
#endif

# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

## There are only two places that call swtch:
## 1. In proc.c:scheduler where swtch is called to save context of scheduler and switch to
##    newly selected proc that is ready to run
## 2. In proc.c:sched which is generally used to switch kernel to the context of scheduler
##    where sched is called
##
## sched is generaly called when the current proc cannot continue running and need give up
## CPU. There are 3 places that call shced:
## 1. exit
## 2. yield
## 3. sleep

.globl swtch
swtch:
  movl 4(%esp), %eax  ## Old context
  movl 8(%esp), %edx  ## New context

  # Save old callee-saved registers
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  movl %esp, (%eax) ## Save the addr of saved kernel context to the pointer to old old context
  movl %edx, %esp   ## Switch to new stack

  # Load new callee-saved registers
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret       ## When return, it will return to the new context where the new proc calls swtch

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

extern struct proc proc[NPROC];

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_co_yield(void)
{
  // PART 1
  //  get args
  int pid; // other->pid
  int value;
  argint(0, &pid);
  argint(1, &value);

  // check pid positivity
  if (pid < 1)
    return -1;

  // check pid not equal to caller pid
  struct proc *p = myproc();
  acquire(&p->lock);
  if (pid == p->pid)
  {
    release(&p->lock);
    return -1;
  }
  release(&p->lock);

  // check pid is existing and not killed
  struct proc *other;
  int found_not_killed = 0;
  //go over all the possible proc-candidates
  for (other = proc; other < &proc[NPROC] && !found_not_killed; other++)
  {
    // avoid panic: for reaquiring when p == other
    if (other == p)
    {
      continue;
    }
    // create order to avoid deadlock - not for 1CPU but as a best prectice
    if (p > other)
    {
      acquire(&other->lock);
      acquire(&p->lock);
    }
    else
    {
      acquire(&p->lock);
      acquire(&other->lock);
    }

    if (other->pid == pid && other->killed == 0)
    {
      found_not_killed = 1;
      // if found - do not release the locks
      break;
    }
    //otherwise - not found, release and return
    release(&other->lock);
    release(&p->lock);
  }
  if (!found_not_killed)
    return -1;

  // mark yourself as ready for co_yield
  p->trapframe->a0 = pid;

  //the good case: other is ready for us to co_yeild
  if (other->state == SLEEPING && other->chan == other && other->trapframe->a0 == p->pid)
  {

    // pass the value to other
    other->trapframe->a1 = value;

    //change states and avoid the RUNNABLE state 
    other->state = RUNNING;
    p->state = SLEEPING;
    p->chan = p;

    // bypass scheduler by updating the cpu running process
    mycpu()->proc = other;

    //just before switching, leaving only other lock in our hand
    release(&p->lock);

    // direct context switch
    swtch(&p->context, &other->context);

    //p wakes up here when another process directly pass control to it
    //p->lock passed by other and we release it now
    release(&p->lock);
  }
  else
  {
    // other is not ready for co_yield and we go to sleep
    // release other lock so scheduler can take him
    release(&other->lock);

    p->state = SLEEPING;
    p->chan = p;

    //pass control through scheduler, allowing him to initiate other
    sched();

    //here we wake up from other switch directly 
    //before returning the value we mark ourselves as not ready
    p->trapframe->a0 = 0;
    
    //p wakes up from sched() becease switch direct so its lock is taken and we should release it
    release(&p->lock);
  }

  //release channel
  p->chan = 0;

  // other left its value in our a1 register
  return p->trapframe->a1;
  //after we return, other will wake up by scheduler some day
  //we wil return to userspace
  //recalling co_yield from the loop will send us to the same dance, over and over...
  //eventullay no one will reach the first sched() but the first process to co_yield
}

// scheduler holds p1----------------------
// p1  -> co_yield(p2) p1->a1 = pid2
// p2->a1 \= pid1  RESULT: p1 sleeps + sched()
// scheduler holds p2---------------------
// p2 -> co_yield(p1) p2->a1 = pid1 (signals)
// p1->a1 = pid2 RESULTS  p2 sleeps p2(locks)
// swtch to p1:
// wakes up after sched():
// releases
// return p1->a0
// p1 -> co_yield(p2) p1->a1 = pid2 (signals)
// p2->a1 = pid1 RESULTS  p1 sleeps p1(locks)
// swtch to p2:
// wakes up after swtch
// releases
// return p2->a0
// p2 -> co_yield(p1) p2->a1 = pid1 (signals)
// ....
// scheduler holds p2 now and on
// once timer interrupt is received, scheduler will try to re-release p2 lock specifically
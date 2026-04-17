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
  for (other = proc; other < &proc[NPROC] && !found_not_killed; other++)
  {
    // panic: aquire solution for reaquiring when p == other
    if (other == p)
    {
      continue;
    }
    // create order to avoid deadlock
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
    release(&other->lock);
    release(&p->lock);
  }
  if (!found_not_killed)
    return -1;

  // 3. Check for the Rendezvous
 p->trapframe->a0 = pid; 
  p->trapframe->a1 = value;

  if (other->state == SLEEPING && other->chan == other && other->trapframe->a0 == p->pid) {
      // --- TARGET IS PREPARED: DIRECT SWITCH PATH ---
      
      // Pass the value to the target
      other->trapframe->a1 = value;
      
      // Manually change states, bypassing the RUNNABLE state entirely
      other->state = RUNNING;
      p->state = SLEEPING;
      p->chan = p;

      // Bypass the normal scheduler by manually updating the CPU's running process
      mycpu()->proc = other;

      // LOCK HANDOFF:
      // We currently hold BOTH p->lock and other->lock.
      // We must release p->lock BEFORE switching so we don't hold it indefinitely.
      // We MUST KEEP other->lock held, because 'other' expects it to be held when it wakes up!
      release(&p->lock);

      // Perform the direct process-to-process context switch
      swtch(&p->context, &other->context);

      // When 'p' eventually wakes up here later, it means another process directly 
      // switched back to it. That other process left p->lock held, so we must release it now.
      release(&p->lock);
      
  } else {
      // --- TARGET NOT PREPARED: SCHEDULER PATH ---
      
      // Target is not waiting for us. We must sleep until it is prepared.
      // We don't need the target's lock to go to sleep, so release it.
      release(&other->lock);
      
      p->state = SLEEPING;
      p->chan = p;
      
      // Go to sleep via the normal scheduler
      sched();
      
      // When 'p' wakes up from sched(), its lock will be held.
      release(&p->lock);
  }

  // Cleanup and return
  p->chan = 0;
  
  // If we got here, 'other' left its value in our a1 register
  return p->trapframe->a1;
}
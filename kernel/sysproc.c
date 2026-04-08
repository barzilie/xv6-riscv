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
  //PART 1
  // get args
  int pid;
  int value;
  argint(0, &pid);
  argint(1, &value);

  //check pid positivity
  if(pid < 1) return -1;

  //check pid not equal to caller pid
  struct proc *p = myproc();
  acquire(&p->lock);
  if (pid == p->pid){
    release(&p->lock);
    return -1;
  } 
  
  //check pid is existing and not killed
  struct proc *other;
  int found_not_killed = 0;
  for (other = proc; other < &proc[NPROC] && !found_not_killed; other++)
  {
    acquire(&other->lock);
    if (other->pid == pid && other->killed == 0)
    {
      found_not_killed = 1;
      //if found - do not release the lock
      break;
    }
    release(&other->lock);
  }
  if(!found_not_killed) return -1;

  //PART 2
  //co_yield logic:
  


  //do not forget to release p, other locks

}

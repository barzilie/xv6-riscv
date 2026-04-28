#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{

  printf("error test case 1: PID not exist\n");
  printf("CALLING: co_yield(1234567, 1)\n");
  printf("co yeild returnd: %d\n",co_yield(1234567, 1));
  printf("------------------------------------------\n");

  printf("error test case 2: PID killed\n");
  int killed_pid2 = fork();
  //kill child
  if(killed_pid2 == 0){
    exit(0);
  }
  else{
    wait(0);
  }
  printf("CALLING: co_yield(killed_pid2, 1)\n");
  printf("co yeild returnd: %d\n",co_yield(killed_pid2, 1));
  printf("------------------------------------------\n");

  printf("error test case 3: self co_yeild\n");
  printf("CALLING: co_yield(getpid(), 1)\n");
  printf("co yeild returnd: %d\n",co_yield(getpid(), 1));
  printf("------------------------------------------\n");

  printf("base succesfull test case: co_yeild\n");
  printf("------------------------------------------\n");

  int pid1 = getpid(); // Parent PID
  int pid2 = fork();   // Child PID
  if (pid2 == 0)
  { // Child
    for (;;)
    {
      int value = co_yield(pid1, 1);
      printf("Child received: %d\n", value); // Should print 2
    }
  }
  else
  { // Parent
    for (;;)
    {
      int value = co_yield(pid2, 2);
      printf("parent received: %d\n", value); // Should print 1
    }
  }

  exit(0);
}

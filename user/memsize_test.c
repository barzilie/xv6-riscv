#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int sz = memsize();
  fprintf(1,"number of bytes used: %d\n",sz);
  void* m = malloc(20000);
  sz = memsize();
  fprintf(1,"number of bytes used after allocation: %d\n",sz);
  free(m);
  sz = memsize();
  fprintf(1,"number of bytes used after free: %d\n",sz);
  exit(0);
}

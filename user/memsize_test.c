#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define KB 1024
#define MB (1024 * KB)

int main() {
  int mem_before, mem_after;
  
  mem_before = memsize();
  printf("Memory usage before allocation: %d bytes\n", mem_before); // task 2.5.1

  char *buffer = malloc(20 * KB); // task 2.5.2
  if (!buffer) {
    printf("Memory allocation failed.\n");
    exit(0);
  }

  mem_after = memsize();
  printf("Memory usage after allocation: %d bytes\n", mem_after); // task 2.5.3

  free(buffer); // task 2.5.4

  mem_after = memsize();
  printf("Memory usage after deallocation: %d bytes\n", mem_after); // task 2.5.5

  exit(0);
}


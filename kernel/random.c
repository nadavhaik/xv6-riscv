
// ASSINGMENT 4

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"


struct spinlock lock;

uint8 seed;

// Linear feedback shift register
// Returns the next pseudo-random number
// The seed is updated with the returned value
uint8 lfsr_char(uint8 lfsr)
{
  uint8 bit;
  bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
  lfsr = (lfsr >> 1) | (bit << 7);
  return lfsr;
}

// read (int, uint64, int)
int randomread(int fd, uint64 dst, int n){
    int target = n;

    acquire(&lock);

    while (n > 0)
    {
      seed = lfsr_char(seed);
      
      if(either_copyout(fd, dst, &seed, 1) == -1) {
        release(&lock);
        return target - n;
      }

      n--;
      dst++;
    }
  
    release(&lock);

    return target - n;
}

int randomwrite(int fd, uint64 src, int n){
  if (n != 1) return -1;

  int could_get_seed = either_copyin(&seed, src, src, n) != -1;
  
  if (!could_get_seed) 
    return -1;

  return 1;
}

void randominit(void)
{
  seed = 0x2A;

  devsw[RANDOM].read = randomread;
  
  devsw[RANDOM].write = randomwrite;

  initlock(&lock, "random_lock");
}

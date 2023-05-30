#include "types.h"
#include "syscall.h"
#include "random.h"

// credit: https://stackoverflow.com/questions/24005459/implementation-of-the-random-number-generator-in-c-c

extern uint64 sys_uptime(void);


char is_seed_initialized = 0;
static uint64 next = 1;

void __srand(unsigned int seed) 
{ 
    next = seed; 
}


uint64 irand(void)
{ 
    if(!is_seed_initialized) {
        __srand(sys_uptime());
        is_seed_initialized = 1;
    }
    next = next * 1103515245 + 12345; 
    return (uint64)(next/65536) % RAND_MAX; 
}

float frand(void)
{
    return ((float) irand()) / RAND_MAX;
}

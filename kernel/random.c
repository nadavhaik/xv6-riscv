#include "random.h"
#include "types.h"



// credit: https://stackoverflow.com/questions/24005459/implementation-of-the-random-number-generator-in-c-c



char is_seed_initialized = 0;
static uint64 next = 1;

void __srand(unsigned int seed) 
{ 
    next = seed; 
}


uint64 irand(void)
{ 
    if(!is_seed_initialized) {
        __srand(0);
        is_seed_initialized = 1;
    }
    next = next * 1103515245 + 12345; 
    return (uint64)(next/65536) % RAND_MAX; 
}

uint64 random_in_range(uint64 min, uint64 max)
{
    return (irand() * (max - min)) / RAND_MAX + min; 
}
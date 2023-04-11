#include "types.h"

void swap(void* a, void* b, uint64 size)
{
    char* _a = (char*) a;
    char* _b = (char*) b;
    for(int i=0; i<size; i++)
    {
        char tmp = *(_a + i);
        *(_a + i) = *(_b + i);
        *(_b + i) = tmp;
    }
}


void* __arr_at(void* array, uint64 i, uint64 size)
{
    return (void*) (((char*) array) + size * i);
}


void bsort(void *base, uint64 nitems, uint64 size, int (*compar)(const void *, const void*)) 
{
    for(uint64 i=0; i<nitems; i++)
    {
        for(uint64 j=i+1; j<nitems;j++)
        {
            if(compar(__arr_at(base, i, size), __arr_at(base, j, size)) < 0)
            {
                swap(__arr_at(base, i, size), __arr_at(base, j, size), size);
            }
        }
    }
}
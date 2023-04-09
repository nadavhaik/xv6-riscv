#include "types.h"

#define SWAP(a, b, size)						      \
  do									      \
    {									      \
      uint64 __size = (size);						      \
      char *__a = (a), *__b = (b);					      \
      do								      \
	  {								      \
	    char __tmp = *__a;						      \
	    *__a++ = *__b;						      \
	    *__b++ = __tmp;						      \
	  } while (--__size > 0);						      \
    } while (0)


void* __arr_at(void* array, uint64 i, uint64 size)
{
    return (void*) (((char*) array) + size * i);
}

uint64 __qsort__partiton(void* array, uint64 size, uint64 low, uint64 high, int (*compar)(const void *, const void*))
{
    void* pivot = ((char* ) array) + (high * size);
    uint64 i = low - 1;
    for(int j = low; j < high; j++)
    {
        if(compar(__arr_at(array, j, size), pivot) <= 0)
        {
            i++;
            SWAP(__arr_at(array, i, size), __arr_at(array, j, size), size);
        }
    }
    SWAP(__arr_at(array, i+1, size), __arr_at(array, high, size), size);
    return i+1;
}

void __qsort(void *base, uint64 size, uint64 low, uint64 high,
int (*compar)(const void *, const void*))
{
    if(low < high)
    {
        uint64 pivot = __qsort__partiton(base, size, low, high, compar);
        __qsort(base, size, low, pivot-1, compar);
        __qsort(base, size, pivot+1, high, compar);
    }
} 


void qsort(void *base, uint64 nitems, uint64 size, int (*compar)(const void *, const void*)) 
{
    __qsort(base, size, 0, nitems-1, compar);
}

void bsort(void *base, uint64 nitems, uint64 size, int (*compar)(const void *, const void*)) 
{
    for(uint64 i=0; i<nitems; i++)
    {
        for(uint64 j=0; j<nitems;j++)
        {
            if(compar(__arr_at(base, i, size), __arr_at(base, j, size)) < 0)
            {
                SWAP(__arr_at(base, i, size), __arr_at(base, j, size), size);
            }
        }
    }
}


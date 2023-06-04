#include "user/user.h"

int main()
{
    int size = 1;
    for(int i=0; i<30; i++)
    {
        printf("mallocing %d\n", size);
        malloc(size);
        size *= 2;
    }

    return 0;
}
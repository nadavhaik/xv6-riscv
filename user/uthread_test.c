#include "kernel/types.h"
#include "kernel/extra_types.h"
#include "user/user.h"
#include "user/uthread.h"


void print_abc()
{
    for(char c='a'; c < 'z'; c++)
    {
        printf("%c\n", c);
        uthread_yield();
    }
}

void print_numbers_to_100()
{
    for(int i=1; i<=100;i++)
    {
        printf("%d\n", i);
        uthread_yield();
    }
}

void main()
{
    uthread_create(print_numbers_to_100, HIGH);
    uthread_create(print_abc, HIGH);
    uthread_start_all();
}
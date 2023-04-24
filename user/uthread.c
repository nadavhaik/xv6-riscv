#include "user/uthread.h"
#include "user/user.h"

static int uthreads_initialized = 0;
static struct uthread uthreads[MAX_UTHREADS];

void initialize_uthreads()
{
    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        t->state = FREE;
    }
    uthreads_initialized = 1;
}

int uthread_create(void (*start_func)(), enum sched_priority priority)
{
    if(!uthreads_initialized) initialize_uthreads();
    
    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        if(t->state != FREE) continue; 

        // else - t is a suitable thread entry in the table
        t->context.ra = (uint64) start_func;
        t->priority = priority;

        memcpy(t->ustack, &uthread_exit, sizeof(&uthread_exit));
        memcpy(&t->ustack[sizeof(&uthread_exit)], start_func, sizeof(start_func));
        t->context.sp = (uint64)&t->ustack[sizeof(&uthread_exit)];
        // t->context.sp = (uint64)&t->ustack[sizeof(&uthread_exit) + sizeof(start_func)];
        // t->context.sp = (uint64)&t->ustack[sizeof(&uthread_exit)];
        
        t->state = RUNNABLE;
        return 0;
    }

    return -1;
}

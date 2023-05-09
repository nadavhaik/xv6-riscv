#include "user/uthread.h"
#include "user/user.h"
#include "kernel/extra_types.h"

#define ADDRESS_SIZE sizeof(void*)


static bool uthreads_table_initialized = false;
static struct uthread uthreads[MAX_UTHREADS];
static struct uthread* current_thread = NULL;


void set_current_thread(struct uthread* t) 
{
    // printf("called set_current_thread\n");
    current_thread = t;
    // printf("set_current_thread ended\n");
}

void uthread_print(struct uthread* t)
{
    printf("thread at %p with priority %d and state %d\n",t, t->priority, t->state);
}

void print_all_uthreads()
{
    printf("all threads:\n");
    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
        uthread_print(t);
}


void print_ctx(struct context* ctx)
{
    printf("CONTEXT AT %l:\n", ctx);
    printf("ra=%lu\n", ctx->ra);
    printf("sp=%lu\n", ctx->sp);
    printf("*sp=%lu\n", *((uint64*)ctx->sp));
    printf("s0=%lu\n", ctx->s0);
    printf("s1=%lu\n", ctx->s1);
    printf("s2=%lu\n", ctx->s2);
    printf("s3=%lu\n", ctx->s3);
    printf("s4=%lu\n", ctx->s4);
    printf("s5=%lu\n", ctx->s5);
    printf("s6=%lu\n", ctx->s6);
    printf("s7=%lu\n", ctx->s7);
    printf("s8=%lu\n", ctx->s8);
    printf("s9=%lu\n", ctx->s9);
    printf("s9=%lu\n", ctx->s9);
    printf("s10=%lu\n", ctx->s10);

}

void raise_unimplemented_error(char* function_name) 
{
    printf("ERROR: %s IS NOT IMPLEMENTED!\n", function_name);
    exit(-1);
}

void initialize_uthreads_table()
{
    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        memset(&t->context, 0, sizeof(t->context));
        t->state = FREE;
    }
    uthreads_table_initialized = true;
}

int uthread_create(void (*start_func)(), enum sched_priority priority)
{
    if(!uthreads_table_initialized) initialize_uthreads_table();
    
    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        if(t->state != FREE) continue;

        // else - t is a suitable thread entry in the table
        t->context.ra = (uint64) start_func;
        t->priority = priority;

        char* sp = &t->ustack[STACK_SIZE];
        memcpy(sp - ADDRESS_SIZE, (void*)uthread_exit, ADDRESS_SIZE);
        // sp -= ADDRESS_SIZE;
        // memcpy(sp, (void*)start_func, ADDRESS_SIZE);
        // sp -= ADDRESS_SIZE;

        // memcpy(sp, (void*) &uthread_exit, ADDRESS_SIZE);
        // sp += ADDRESS_SIZE;
        // memcpy(sp, (void*) start_func, ADDRESS_SIZE);
        // sp += ADDRESS_SIZE


        t->context.sp = (uint64)sp;
        t->state = RUNNABLE;
        
        return 0;
    }

    return -1;
}

void print_thread(struct uthread* t)
{
    printf("state=%d, priority=%d\n", (int) t->state, (int) t->priority);
}

typedef struct uthread_dynamic_array
{
    struct uthread* data[MAX_UTHREADS];
    int size;
} uthread_dynamic_array;


uthread_dynamic_array find_prioritized_uthreads()
{
    // printf("called find_prioritized_uthreads\n");

    uthread_dynamic_array res;
    res.size = 0;
    int current_priority = -1;
    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        if (t->state != RUNNABLE || ((int)t->priority) < current_priority) 
            continue;

        if(((int)t->priority) > current_priority) 
        {
            current_priority = t->priority;
            res.size = 0;
        } 

        res.data[res.size] = t;
        res.size++;
    }

    // printf("finished find_prioritized_uthreads\n");
    // printf("res.data[0]=");
    // print_thread(res.data[0]);
    return res;
}



void uthread_sched(struct uthread* t)
{
    // printf("called uthread_sched for thread in address %lx\n", (uint64)t);
    struct uthread* current = uthread_self();

    // if current context is NULL: (main thread) -
    // context won't be backed up and will be stored temporarly as garbage
    // to avoid segfaults.

    struct context* current_context;
    struct context new_context;
    
    if(current == NULL) 
    {
        memset(&new_context, 0, sizeof(new_context));
        current_context = &new_context;
    } else
    {
        if(current->state == RUNNING) current->state = RUNNABLE;
        current_context = &current->context;
    }
        
    t->state = RUNNING;
    
    
    set_current_thread(t);

    uswtch(current_context, &t->context);    
}

void uthread_scheduler()
{
    // for(;;)
    {
        uthread_dynamic_array prioritized_uthreads = find_prioritized_uthreads();
        if(prioritized_uthreads.size == 0) exit(0);
        struct uthread* current_thread = uthread_self();
        
        // trying to find a pointer for current_thread in highest_priority_uthreads:
        struct uthread** current_tpp = NULL;
        int current_tpp_index = -1;
        for(struct uthread** tpp = prioritized_uthreads.data;
         tpp < &prioritized_uthreads.data[prioritized_uthreads.size];
         tpp++)
        {
            current_tpp_index++;
            if(*tpp == current_thread) 
            {
                // printf("current tpp in prioritized_uthreads! address=%lx\n", (uint64)current_thread);
                current_tpp = tpp;
                break;
            }
        }

        // if current thread is not in highest_priority_uthreads:
        if(current_tpp == NULL) 
        {
            // printf("current tpp not in prioritized_uthreads! address=%lx\n", (uint64)current_thread);
            uthread_sched(prioritized_uthreads.data[0]);
            return;
        }
        // printf("current_tpp != NULL!\n");

        // printf("current_tpp_index=%d\n", current_tpp_index);
        
        uthread_sched(prioritized_uthreads.data[(current_tpp_index + 1) % prioritized_uthreads.size]);
    }
}


void uthread_yield() 
{
    // printf("called uthread_yield\n");
    struct uthread* current = uthread_self();
    if(current != NULL) current->state = RUNNABLE;
    uthread_scheduler();
}


enum sched_priority uthread_set_priority(enum sched_priority priority) 
{ 
    struct uthread* current_thread = uthread_self();

    enum sched_priority previous_priority = current_thread->priority;
    current_thread->priority = priority;

    return previous_priority;
}

enum sched_priority uthread_get_priority(){ return uthread_self()->priority; }

int uthread_start_all() 
{ 
    if(uthread_self() != NULL) return -1;
    uthread_dynamic_array prioritized_uthreads = find_prioritized_uthreads();


    for(;;)
    {
        for(struct uthread** tpp = prioritized_uthreads.data; tpp < &prioritized_uthreads.data[prioritized_uthreads.size]; tpp++)
        {
            uthread_sched(*tpp);
        }
    }

    return 0;
}


void uthread_exit() 
{
    struct uthread* curr_t = uthread_self();
    curr_t->state = FREE;
    uthread_scheduler();
}

struct uthread* uthread_self()
{
    return current_thread;
}

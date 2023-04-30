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
    // printf("called initialize_uthreads_table\n");

    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        memset(&t->context, 0, sizeof(t->context));
        t->state = FREE;
    }
    uthreads_table_initialized = true;
}

void uthread_push(char** spp, void* content)
{
    // printf("called uthread_push\n");
    memcpy(*spp, content, ADDRESS_SIZE);
    *spp += ADDRESS_SIZE;
}
void uthread_pop_many(char** spp, int times) {*spp -= ADDRESS_SIZE * times;}
void uthread_pop(char** spp) {uthread_pop_many(spp, 1);}
void* uthread_peek(char** spp) {return *spp;}

int uthread_create(void (*start_func)(), enum sched_priority priority)
{
    printf("called uthread_create\n");
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

        // memcpy(sp - ADDRESS_SIZE, (void*)start_func, ADDRESS_SIZE);
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

#define uthread_sched2(_t) \
{ \
     printf("called uthread_sched\n"); \
     struct uthread* t = _t; \
     struct uthread* current = uthread_self(); \
    struct uthread garbage_thread; \
    if(current == NULL) current = &garbage_thread; \
    current->state = RUNNABLE;  \
    t->state = RUNNING; \
    set_current_thread(t); \
    uswtch(&current->context, &t->context); \
}

void uthread_sched(struct uthread* t)
{
    // printf("called uthread_sched\n");
    struct uthread* current = uthread_self();



    // if current context is NULL: (main thread) -
    // context won't be backed up and will be stored temporarly as garbage
    // to avoid segfaults.

    struct context current_context;
    
    if(current == NULL) 
    {
        memset(&current_context, 0, sizeof(current_context));
    } else
    {
        current->state = RUNNABLE;
        current_context = current->context;
    }
        
    t->state = RUNNING;
    
    set_current_thread(t);


    uswtch(&current_context, &t->context);
}

void uthread_scheduler()
{
    // printf("called uthread_scheduler\n");
    for(;;)
    {
        uthread_dynamic_array prioritized_uthreads = find_prioritized_uthreads();
        // printf("number_of_uthreads=%d\n", prioritized_uthreads.size);
        if(prioritized_uthreads.size == 0) continue;

        struct uthread* current_thread = uthread_self();

        // first run (main thread):
        if(current_thread == NULL) 
        {
            // printf("current_thread == NULL!\n");
            struct uthread* t0 = prioritized_uthreads.data[0];
            // printf("calling usched\n");
            // print_thread(t0);
            uthread_sched(t0);
        }
        

        // trying to find a pointer for current_thread in highest_priority_uthreads:
        struct uthread** current_tpp = NULL;
        for(struct uthread** tpp = prioritized_uthreads.data; tpp < &prioritized_uthreads.data[prioritized_uthreads.size]; tpp++)
        {
            if(*tpp == current_thread) 
            {
                current_tpp = tpp;
                break;
            }
        }

        // if current thread is not in highest_priority_uthreads:
        if(current_tpp == NULL) uthread_sched(prioritized_uthreads.data[0]);
        // printf("current_tpp != NULL!\n");

        int current_tpp_index = (current_tpp - prioritized_uthreads.data) / sizeof(struct uthread**);
        
        uthread_sched(prioritized_uthreads.data[(current_tpp_index + 1) % prioritized_uthreads.size]);
    }
}


void uthread_yield() 
{
    struct context current_ctx;
    
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

    uthread_scheduler();
    return 0;
}

void uthread_exit() 
{
    struct uthread* curr_t = uthread_self();

    if(curr_t == NULL) exit(0);
    curr_t->state = FREE;

    uthread_scheduler();
}

struct uthread* uthread_self()
{
    // printf("called uthread_self\n");
    return current_thread;
}

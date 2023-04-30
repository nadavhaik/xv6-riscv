#include "user/uthread.h"
#include "user/user.h"
#include "kernel/extra_types.h"

#define ADDRESS_SIZE sizeof(void*)


static bool uthreads_table_initialized = false;
static struct uthread uthreads[MAX_UTHREADS];
static struct uthread* current_thread = NULL;


void set_current_thread(struct uthread* t) {current_thread = t;}

void raise_unimplemented_error(char* function_name) 
{
    printf("ERROR: %s IS NOT IMPLEMENTED!\n", function_name);
    exit(-1);
}

void initialize_uthreads_table()
{
    printf("called initialize_uthreads_table\n");

    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        t->state = FREE;
    }
    uthreads_table_initialized = true;
}

void uthread_push(char** spp, void* content)
{
    printf("called uthread_push\n");
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

        char* sp = t->ustack;
        char** spp = &sp;

        uthread_push(spp, (void*) &uthread_exit);
        uthread_push(spp, (void*) start_func);


        t->context.sp = (uint64)(*spp);
        t->state = RUNNABLE;
        
        return 0;
    }

    return -1;
}

// typedef struct sized_uthreads_array {
//     int size;
//     struct uthread* array;
// } sized_uthreads_array;

int find_prioritized_uthreads(struct uthread** res)
{
    int current_size = 0;
    int current_priority = -1;
    for(struct uthread* t = uthreads; t < &uthreads[MAX_UTHREADS]; t++)
    {
        if(t->priority < current_priority || t->state != RUNNABLE) continue;
        if(t->priority > current_priority) current_size = 0;

        res[current_size++] = t;
    }

    return current_size;
}

void usched(struct uthread* t)
{
    printf("called usched\n");

    struct uthread* current = uthread_self();

    // if current context is NULL: (main thread) -
    // context won't be backed up and will be stored temporarly as garbage
    // to avoid segfaults.
    struct uthread garbage_thread;
    if(current == NULL) current = &garbage_thread;

    current->state = RUNNABLE;
    t->state = RUNNING;
    set_current_thread(t);
    uswtch(&current->context, &t->context);
}

void uthread_scheduler()
{
    printf("called uthread_scheduler\n");
    for(;;)
    {
        struct uthread* highest_priority_uthreads[MAX_UTHREADS];
        int number_of_uthreads = find_prioritized_uthreads(highest_priority_uthreads);
        if(number_of_uthreads == 0) continue;

        struct uthread* current_thread = uthread_self();

        // first run (main thread):
        if(current_thread == NULL) usched(highest_priority_uthreads[0]);
        

        // trying to find a pointer for current_thread in highest_priority_uthreads:
        struct uthread** current_tpp = NULL;
        for(struct uthread** tpp = highest_priority_uthreads; tpp < &highest_priority_uthreads[number_of_uthreads]; tpp++)
        {
            if(*tpp == current_thread) 
            {
                current_tpp = tpp;
                break;
            }
        }

        // if current thread is not in highest_priority_uthreads:
        if(current_tpp == NULL) usched(highest_priority_uthreads[0]);

        int current_tpp_index = (current_tpp - highest_priority_uthreads) / sizeof(struct uthread**);
        
        usched(highest_priority_uthreads[(current_tpp_index + 1) % number_of_uthreads]);
    }
}


void uthread_yield() 
{
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
    return current_thread;
}

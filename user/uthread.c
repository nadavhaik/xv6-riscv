#include "user/uthread.h"
#include "user/user.h"
#include "kernel/extra_types.h"

#define ADDRESS_SIZE sizeof(void*)


static bool uthreads_table_initialized = false;
static struct uthread uthreads[MAX_UTHREADS];
static uthreads_list* threads_list = NULL;

void set_current_thread(struct uthread* t)
{
    uthreads_list* current_thread_list = malloc(sizeof(uthreads_list));
    current_thread_list->data = t;
    current_thread_list->next = threads_list;
    threads_list = current_thread_list;
}

void pop_thread()
{
    uthreads_list* current_thread_list = threads_list->next;
    free(threads_list);
    threads_list = current_thread_list;
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
        t->state = FREE;
    }
    uthreads_table_initialized = true;
}

void uthread_push(char** spp, void* content)
{
    *spp -= ADDRESS_SIZE;
    memcpy(*spp, content, ADDRESS_SIZE);
}

void uthread_pop_many(char** spp, int times)
{
    *spp += ADDRESS_SIZE * times;
}

void uthread_pop(char** spp)
{
    uthread_pop_many(spp, 1);
}

void* uthread_peek(char** spp)
{
    return *spp + ADDRESS_SIZE;
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

        char* sp = &t->ustack[STACK_SIZE-1];
        char** spp = &sp;

        uthread_push(spp, (void*) &uthread_exit);
        uthread_push(spp, (void*) start_func);

        t->context.sp = (uint64)*spp;
        t->state = RUNNABLE;
        set_current_thread(t);
        
        return 0;
    }

    return -1;
}

// typedef struct sized_uthreads_array {
//     int size;
//     struct uthread* array;
// } sized_uthreads_array;

int get_highest_priority_uthreads(struct uthread** res)
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

static struct context* previous_context;

void uthread_yield() 
{
    struct uthread* highest_priority_uthreads[MAX_UTHREADS];
    int number_of_uthreads = get_highest_priority_uthreads(highest_priority_uthreads);

    // intr_on();
    for(struct uthread** tpp = highest_priority_uthreads; tpp < &highest_priority_uthreads[number_of_uthreads]; tpp++)
    {
        struct uthread* t = *tpp;
        struct uthread* current = uthread_self();
        t->state = RUNNING;
        set_current_thread(t);
        uswtch(&current->context, &t->context);
    }
}


enum sched_priority uthread_set_priority(enum sched_priority priority) 
{
    uthread_self()->priority = priority;
}

enum sched_priority uthread_get_priority()
{
    return uthread_self()->priority;
}

int uthread_start_all() 
{
    
    for(;;) uthread_yield();
}

void uthread_exit() 
{
    struct uthread* curr_t = uthread_self();

    if(curr_t == NULL) exit(0);
    curr_t->state = FREE;
    pop_thread();
    struct uthread* new_t = uthread_self();
    if(new_t == NULL) exit(0);

    uswtch(&curr_t->context, &new_t->context);
}

struct uthread* uthread_self()
{
    if(threads_list == NULL) return NULL;
    return threads_list->data;
}

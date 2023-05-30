#include "ustack.h"

static ustack *global_ustack = 0;

void* __ustack_my_malloc(uint buffer_size)
{
    char* page_end = sbrk(buffer_size);
    char* new_allocated = page_end;

    return (void*) new_allocated;
}

ustack* __ustack_peek()
{
    return global_ustack;
}

ustack* __ustack_push(uint buffer_size)
{
    ustack *new_node = __ustack_my_malloc(sizeof(ustack));
    void *buffer = __ustack_my_malloc(buffer_size);

    new_node->buffer_size = buffer_size;
    new_node->buffer = buffer;
    new_node->prev = global_ustack;

    global_ustack = new_node;
    return new_node;
}

ustack* ustack_pop()
{
    if (global_ustack == 0)
        return (ustack *)-1;
    ustack *popped = global_ustack;
    global_ustack = global_ustack->prev;
    return popped;
}

void* ustack_malloc(uint len)
{
    if (len > MAX_ALLOCATION_SIZE)
        return (void *)-1;
    return __ustack_push(len)->buffer;
}

int ustack_free()
{
    ustack* top = __ustack_peek();
    if (top == 0)
        return -1;
    ustack_pop();
    sbrk(-top->buffer_size -((int)sizeof(ustack)));
    return 0;
}
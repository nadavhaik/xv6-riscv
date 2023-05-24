#include "kernel/types.h"
// #include "kernel/riscv.h"
#include "user/user.h"

#define MAX_ALLOCATION_SIZE 512 // bytes

typedef struct ustack
{
    struct ustack* prev;
    uint buffer_size;
    void* buffer;
} ustack;

void*   ustack_malloc(uint len);
int     ustack_free();
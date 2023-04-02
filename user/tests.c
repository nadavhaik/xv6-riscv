#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//(*syscalls[])(void)(
//static uint64 (*syscalls[])(void) = {

#define stdin 0
#define stdout 1
#define stderr 2

#define NULL 0

typedef void (*testfunc)(void);

typedef struct Test {
    char* name;
    testfunc foo;
    int timeout;
} Test;

static int default_timeout = 10;
void runt(Test test)
{
    printf("Running %s\n", test.name);
    int* child_done = malloc(sizeof(int));
    *child_done = 0;
    int pid = fork();
    if(pid == 0) {
        test.foo();
        printf("%s finished successfully!\n", test.name);
        *child_done = 1;
    } else {
        int timeout = test.timeout > 0 ? test.timeout : default_timeout;
        for(int time_slept = 0; *child_done == 0 && time_slept < timeout; time_slept++)
        {
            sleep(1);
        }
        if(*child_done == 0)
        {
            fprintf(stderr, "Test %s failed or timed out!\n", test.name);
        }
    }
}



void assert_ints_equal(int x, int y)
{
    if(x == y) return;
    fprintf(stderr, "Assertion failed! Expected: %d, got: %d\n", x, y);
    exit(0);
}

void assert_lt_ints(int x, int y) {
    if(x < y) return;
    fprintf(stderr, "Assertion failed! Condition: %d < %d isn't satisfied\n", x, y);
    exit(0);
}

void assert_lte_ints(int x, int y) {
    if(x <= y) return;
    fprintf(stderr, "Assertion failed! Condition: %d <= %d isn't satisfied\n", x, y);
    exit(0);
}

void assert_strings_equal(char* s1, char* s2) 
{
    if(strcmp(s1, s2) == 0) return;
    fprintf(stderr, "Assertion failed! Expected: %s, got: %s\n", s1, s2);
    exit(0);
}

void print_memsize()
{
    printf("memsize=%d\n", memsize());
}

void test_q2() 
{
    int m0 = memsize();
    assert_ints_equal(m0, memsize());
    void* p1 = malloc(20);
    int m1 = memsize();
    assert_lte_ints(m0, m1);
    void* p2 = malloc(100000);
    int m2 = memsize();
    assert_lte_ints(m1, m2);
    free(p1);
    assert_ints_equal(m2, memsize());
    free(p2);
    assert_ints_equal(m2, memsize());
}

#define END_OF_TESTS_LIST "%!END_OF_TESTS_LIST%!"

static Test tests[] = {
    {"Test Q2", test_q2, 0},
    {END_OF_TESTS_LIST, NULL, NULL}
};

void run_all_tests()
{
    for(int i=0; strcmp(tests[i].name, END_OF_TESTS_LIST) != 0; i++)
    {
        runt(tests[i]);
    }
}

int main() 
{
    run_all_tests();
    exit(0);
}
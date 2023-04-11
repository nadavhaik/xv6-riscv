#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "../kernel/cfs_stats.h"
#include "user.h"

//(*syscalls[])(void)(
//static uint64 (*syscalls[])(void) = {

int stdin = 0;
int stdout = 1;
int stderr = 2;

#define NULL 0

typedef void (*testfunc)(void);

typedef struct Test {
    char* name;
    testfunc foo;
    int timeout;
} Test;




void assert_ints_equal(int x, int y)
{
    if(x == y) return;
    fprintf(stderr, "Assertion failed! Expected: %d, got: %d\n", x, y);
    exit_nomsg(0);
}

void assert_ints_notequal(int x, int y)
{
    if(x != y) return;
    fprintf(stderr, "Assertion failed! Both ints equal %d\n", x);
    exit_nomsg(0);
}

void assert_lt_ints(int x, int y) {
    if(x < y) return;
    fprintf(stderr, "Assertion failed! Condition: %d < %d isn't satisfied\n", x, y);
    exit_nomsg(0);
}

void assert_lte_ints(int x, int y) {
    if(x <= y) return;
    fprintf(stderr, "Assertion failed! Condition: %d <= %d isn't satisfied\n", x, y);
    exit_nomsg(0);
}

// void assert_int_arrs_equal(int* a1, int* a2, int n)
// {
//     if(memcmp())
// }

void assert_strings_equal(char* s1, char* s2) 
{
    if(strcmp(s1, s2) == 0) return;
    fprintf(stderr, "Assertion failed! Expected: %s, got: %s\n", s1, s2);
    exit_nomsg(0);
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

void test_set_get_cfs_priority()
{
    int pid = getpid();
    assert_ints_equal(set_cfs_priority(-1), -1);
    assert_ints_equal(set_cfs_priority(3), -1);
    for(int priority=0; priority < 3; priority++)
    {
        set_cfs_priority(priority);
        cfs_stats stats; 
        assert_ints_notequal(get_cfs_stats(pid, &stats), -1);
        assert_ints_equal(stats.cfs_priority, priority);
    }
}
#define END_OF_TESTS_LIST "%!END_OF_TESTS_LIST%!"

static Test tests[] = {
    {"Q2", test_q2, 0},
    {"get set cfs priority", test_set_get_cfs_priority, 10000},
    {END_OF_TESTS_LIST, NULL, NULL}
}; 

void run_all_tests()
{
    int number_of_tests = 0;
    for(Test* test = tests; strcmp(test->name, END_OF_TESTS_LIST) != 0; test++)
        number_of_tests++;
    
    int* children_pids = malloc(sizeof(int) * number_of_tests);
    int** children_pipes;
    children_pipes = malloc(sizeof(int*) * number_of_tests);
    for(int** pipearr=children_pipes; pipearr<&children_pipes[number_of_tests]; pipearr++)
    {
        *pipearr = malloc(2 * sizeof(int));
        pipe(*pipearr);
    }

    for(int i=0; strcmp(tests[i].name, END_OF_TESTS_LIST) != 0; i++)
    {
        printf("Running test \"%s\"...\n", tests[i].name);
        int pid;
        if((pid = fork()) == 0) {
            stdout = stderr = children_pipes[i][1];
            tests[i].foo();
            fprintf(stdout, "Test \"%s\" finished successfully!\n", tests[i].name);
            close(stdout);
            exit_nomsg(0);
        } else {
            children_pids[i] = pid;
        }
    }
    int finished_child;
    while ((finished_child = wait_nomsg(0)) > 0)
    {
        int* child_pipe = 0;
        for(int i = 0; i < number_of_tests; i++)
        {
            if(children_pids[i] == finished_child)
            {
                child_pipe = children_pipes[i];
                break;
            }
        }
        char childout[500];
        read(child_pipe[0], childout, 500);
        printf("%s", childout);
    }

    for(int i=0; i<number_of_tests; i++)
    {
        free(children_pipes[i]);
    }
    free(children_pipes);
    free(children_pids);

    printf("\n\nALL TESTS DONE!\n\n");
}

int main() 
{
    run_all_tests();
    exit_nomsg(0);
}
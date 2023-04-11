#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/cfs_stats.h"
#include "user/user.h"



void print_stats(int pid, cfs_stats stats, int out){
    fprintf(out, "PID: %d, CFS priority: %d, run time: %d, sleep time: %d, runnable time: %d\n", pid, stats.cfs_priority, stats.run_time, stats.sleep_time, stats.runnable_time);
}

void test_1(int out){
    for (int i = 0; i < 1000000; i++)
    {
        if(i % 100000 == 0){
            sleep(20);
        }
    }
    cfs_stats stats;
    int pid = getpid();
    if(get_cfs_stats(pid, &stats) != -1){
        print_stats(pid, stats, out);
    }
    close(out);
}
    

#define NUMBER_OF_CHILDREN 3
void main(int argc, char *argv[]) {
    int policy = argc == 0 ? 2 : atoi(argv[0]);
    set_policy(policy);
    int pid;
    int pipes[NUMBER_OF_CHILDREN][2];
    for(int i = 0; i < NUMBER_OF_CHILDREN; i++){
        pipe(pipes[i]);
    }
    for(int i = 0; i < NUMBER_OF_CHILDREN; i++){
        if((pid = fork()) == 0) {
            set_ps_priority(i * 3 + 1);
            set_cfs_priority(i);
            close(pipes[i][0]);
            test_1(pipes[i][1]);
            exit_nomsg(0);
        } 
    }

    while((wait_nomsg(0)) > 0) {}
    for(int i=0; i < NUMBER_OF_CHILDREN; i++)
    {
        char childout[200];
        read(pipes[i][0], childout, 200);
        printf("%s" ,childout);
        close(pipes[i][0]);
    }
}
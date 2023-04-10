#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/cfs_stats.h"
#include "user/user.h"

void print_stats(int pid, cfs_stats stats){
    printf("PID: %d, CFS priority: %d, run time: %d, sleep time: %d, runnable time: %d", pid, stats.cfs_priority, stats.run_time, stats.sleep_time, stats.runnable_time);
}

void test_1(){
    for (int i = 0; i < 1000000; i++)
    {
        if(i % 100000 == 0){
            sleep(1);
        }
    }
    cfs_stats stats;
    int pid = getpid();
    if(get_cfs_stats(pid, &stats) != -1){
        print_stats(pid, stats);
    }
}


    


void main(int argc, char *argv[]) {
    for(int i = 0; i < 3; i++){
        if(fork() == 0) {
            set_cfs_priority(i);
            test_1();
            exit_nomsg(0);
        }
    }
}
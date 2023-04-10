#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/cfs_stats.h"
#include "user/user.h"

// typedef struct mylock
// {
//     int current_owner;
//     int next_user;
// } mylock;

// void init_lock(mylock* lock)
// {
//     lock->current_owner = 0;
//     lock->next_user = 0;
// }

// void aquire(mylock* lock)
// {
//     int my_id = ++(lock->next_user);
//     printf("my_id=%d, current_owner=%d\n", my_id, lock->current_owner);
//     while(my_id < lock->current_owner) {
//         printf("my_id=%d, current_owner=%d\n", my_id, lock->current_owner);
//     }
// }

// void release(mylock* lock)
// {
//     lock->current_owner++;
// }

// mylock printlock;

void print_stats(int pid, cfs_stats stats){
    printf("PID: %d, CFS priority: %d, run time: %d, sleep time: %d, runnable time: %d\n", pid, stats.cfs_priority, stats.run_time, stats.sleep_time, stats.runnable_time);
    
}

void test_1(){
    for (int i = 0; i < 1000000; i++)
    {
        if(i % 100000 == 0){
            sleep(11);
        }
    }
    cfs_stats stats;
    int pid = getpid();
    if(get_cfs_stats(pid, &stats) != -1){
        print_stats(pid, stats);
    }
}


    


void main(int argc, char *argv[]) {
    set_policy(2);
    for(int i = 0; i < 3; i++){
        if(fork() == 0) {
            set_cfs_priority(i);
            test_1();
            exit_nomsg(0);
        } else {
            sleep(100);
        }
    }
    sleep(100);
}
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/cfs_stats.h"
#include "user/user.h"



#define HIGH_PRIORITY_DECAY_FACTOR 75
#define NORMAL_PRIORITY_DECAY_FACTOR 100
#define LOW_PRIORITY_DECAY_FACTOR 125

int decay_factor(const cfs_stats* p)
{
  switch (p->cfs_priority)
  {
  case 0:
    return HIGH_PRIORITY_DECAY_FACTOR;
  case 1:
    return NORMAL_PRIORITY_DECAY_FACTOR;
  case 2:
    return LOW_PRIORITY_DECAY_FACTOR;
  default:
    return -1;
  }
}

int vruntime(const cfs_stats* p)
{
  if(p->run_time == 0 && p->sleep_time == 0 && p->runnable_time == 0)
    return 0;
  return (decay_factor(p) * p->run_time) /
    (p->run_time + p->sleep_time + p->runnable_time); 
}

void print_stats(int pid, cfs_stats stats, int out){
    fprintf(out, "PID: %d, CFS priority: %d, run time: %d, sleep time: %d, runnable time: %d, vruntime: %d\n",
        pid, stats.cfs_priority, stats.run_time, stats.sleep_time, stats.runnable_time, vruntime(&stats));
}

void test_1(int out){
    for (int i = 0; i < 1000000; i++)
    {
        if(i % 100000 == 0){
            sleep(10);
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
            int priority_index = i % 3;
            set_ps_priority(priority_index * 3 + 1);
            set_cfs_priority(priority_index);
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
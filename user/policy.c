#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/cfs_stats.h"
#include "user/user.h"

#define ROUND_ROBIN_POLICY 0
#define PS_POLICY 1
#define CFS_POLICY 2

void main(int argc, char *argv[]){
    if(argc != 2)
        exit(-1, "The program takes exactly one argument");
    
    int policy = atoi(argv[1]);
    if(set_policy(policy) == 0){
        switch (policy)
        {
        case ROUND_ROBIN_POLICY:
            printf("Policy changed successfully to Round Robin policy\n");
            break;
        case PS_POLICY:
            printf("Policy changed successfully to Priority Scheduling policy\n");
            break;
        case CFS_POLICY:
            printf("Policy changed successfully to CFS Priority policy\n");
            break;
        }
    }
    else{
        printf("Error while trying to change policy\n");
    }
}
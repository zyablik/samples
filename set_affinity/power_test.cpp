#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <bitset>
#include <iostream>
#include <string.h>

#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

int main(int argc, char ** argv) {
    if(argc < 2) {
        printf("usage: %s CORE\n", argv[0]);
        exit(1);
    }
    int core = atoi(argv[1]);
    printf("[main] geteuid = %d\n", geteuid());
//    setuid(10117);
//    setuid(99623);
    printf("[main] geteuid = %d\n", geteuid());

    cpu_set_t mask = {};
    CPU_ZERO(&mask);
    CPU_SET(core, &mask);
    if(sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        printf("error in sched_setaffinity() %d: %s\n", errno, strerror(errno));
    }
    printf("[thread = %d] running on cpu #%d\n", gettid(), sched_getcpu());

    struct timeval start_time;
    gettimeofday(&start_time, nullptr);

    int j = 0;
//    daemon(0, 0);
    uint64_t tmp = 3;
//    while(true) {
    for(j == 0; j < 10; j++) {
        for(int i = 0; i < 1000000000; i++) {
            tmp = tmp * (tmp + 1);
        }
        printf("tmp[%d] = %lu\n", j, tmp);
    }
    struct timeval current_time;
    gettimeofday(&current_time, nullptr);
    uint32_t delta_msec = ((current_time.tv_sec - start_time.tv_sec) * 1000000 + (current_time.tv_usec - start_time.tv_usec)) / 1000.0;
    printf("duration = %d millisecs\n", delta_msec);

    return 0;
}

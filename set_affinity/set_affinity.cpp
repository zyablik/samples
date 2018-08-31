#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <bitset>
#include <iostream>
#include <string.h>

cpu_set_t mask = {};

void * thread_func(void *) {
    cpu_set_t  prev_mask = {};
    while(true) {
        if(memcmp(&prev_mask, &mask, sizeof(cpu_set_t)) != 0) {
            if(sched_setaffinity(0, sizeof(mask), &mask) != 0) {
                printf("error in sched_setaffinity() %d: %s\n", errno, strerror(errno));
            }
        }
        printf("[thread = %d] running on cpu #%d\n", gettid(), sched_getcpu());
        sleep(1);
    }

    return NULL;
}

int main() {
    printf("[main] geteuid = %d\n", geteuid());
//    setuid(10117);
    setuid(99623);
    printf("[main] geteuid = %d\n", geteuid());

    pthread_t thread;
    if(pthread_create(&thread, NULL, thread_func, nullptr) != 0) {
        printf("error creating thread %d: %s\n", errno, strerror(errno));
        return 1;
    }

    while(true) {
        for(int i = 0; i < 8; i++) {
            CPU_ZERO(&mask);
            CPU_SET(i, &mask);
            printf("[main = %d] set mask for cpu %d\n", gettid(), i);
            sleep(3);
        }
    }

    return 0;
}

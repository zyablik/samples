#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

/* this function is run by the second thread */
void *inc_x(void *x_void_ptr) {
    /* increment x to 100 */
    int * x_ptr = (int *)x_void_ptr;
    while(++(*x_ptr) < 100);

    printf("[tid = %ld] x increment finished\n", syscall(__NR_gettid));
    printf("[tid = %ld] sleep(10)\n", syscall(__NR_gettid));
    sleep(10);
    printf("[tid = %ld] after sleep\n", syscall(__NR_gettid));

    return NULL;
}

int main() {
    char buf[32];
    int x = 0, y = 0;

    /* show the initial values of x and y */
    printf("[tid = %ld] x: %d, y: %d\n", syscall(__NR_gettid), x, y);

    /* this variable is our reference to the second thread */
    pthread_t inc_x_thread;

    printf("press enter to start thread\n");
    fgets(buf, sizeof(buf), stdin);

    /* create a second thread which executes inc_x(&x) */
    if(pthread_create(&inc_x_thread, NULL, inc_x, &x)) {
        printf("Error creating thread %d: %s\n", errno, strerror(errno));
        return 1;
    }

    /* increment y to 100 in the first thread */
    while(++y < 100);

    printf("[tid = %ld] pthread_join()\n", syscall(__NR_gettid));

    /* wait for the second thread to finish */
    if(pthread_join(inc_x_thread, NULL)) {
        printf("Error joining thread: %d:%s\n", errno, strerror(errno));
        return 2;
    }

    /* show the results - x is now 100 thanks to the second thread */
    printf("[tid = %ld] after join. x: %d, y: %d\n", syscall(__NR_gettid), x, y);
    return 0;
}

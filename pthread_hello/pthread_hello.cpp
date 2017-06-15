#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* this function is run by the second thread */
void *inc_x(void *x_void_ptr) {
    /* increment x to 100 */
    int * x_ptr = (int *)x_void_ptr;
    while(++(*x_ptr) < 100);

    printf("[tid = %d] x increment finished\n", gettid());
    printf("[tid = %d] sleep(10)\n", gettid());
    sleep(10);
    printf("[tid = %d] after sleep\n", gettid());

    return NULL;
}

int main() {
    char buf[32];
    int x = 0, y = 0;

    /* show the initial values of x and y */
    printf("[tid = %d] x: %d, y: %d\n", gettid(), x, y);

    /* this variable is our reference to the second thread */
    pthread_t inc_x_thread;

    printf("press enter to start thread\n");
    gets(buf);

    /* create a second thread which executes inc_x(&x) */
    if(pthread_create(&inc_x_thread, NULL, inc_x, &x)) {
        printf("Error creating thread %d: %s\n", errno, strerror(errno));
        return 1;
    }

    /* increment y to 100 in the first thread */
    while(++y < 100);

    printf("[tid = %d] pthread_join()\n", gettid());

    /* wait for the second thread to finish */
    if(pthread_join(inc_x_thread, NULL)) {
        printf("Error joining thread: %d:%s\n", errno, strerror(errno));
        return 2;
    }

    /* show the results - x is now 100 thanks to the second thread */
    printf("[tid = %d] after join. x: %d, y: %d\n", gettid(), x, y);
    return 0;
}

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

void pexit(const char * format, ...);


pthread_cond_t condition;
pthread_mutex_t mutex;

void *thread_func(void * ptr) {
  printf("before pthread_cond_wait\n");
  int rv = pthread_cond_wait(&condition, &mutex);
  printf("after pthread_cond_wait rv = %d\n", rv);
  return NULL;
}

int main(int argc, char ** argv) {
  pthread_t thread;
  pthread_cond_signal(&condition);
  if(pthread_create(&thread, NULL, thread_func, NULL) != 0)
      pexit("Error creating thread");

  sleep(1);
  pthread_cond_signal(&condition);

  sleep(6);
  return 0;
}

void pexit(const char * format, ...) {
    va_list arglist;
    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
    printf(": errno = %d: %s. exit(1)\n", errno, strerror(errno));
    exit(1);
}

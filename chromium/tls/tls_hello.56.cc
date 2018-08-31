#include <stdio.h>
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local_storage.h"

static base::ThreadLocalStorage::StaticSlot tls_slot = TLS_INITIALIZER;

class ThreadLocalStorageRunner: public base::DelegateSimpleThread::Delegate {
public:
  void Run() override {
    printf("[pthread = %lu] ThreadLocalStorageRunner::Run\n", pthread_self());
    int value = reinterpret_cast<intptr_t>(tls_slot.Get());
    printf("[pthread = %lu] tls_slot.Get() value = %d\n", pthread_self(), value);

    printf("[pthread = %lu] tls_slot.Set() 666\n", pthread_self(), 666);
    tls_slot.Set(reinterpret_cast<void*>(666));
  }
};

void ThreadLocalStorageCleanup(void * value) {
  printf("[pthread = %lu] ThreadLocalStorageCleanup  value = %ld\n", pthread_self(), reinterpret_cast<intptr_t>(value));
  // Tell tls that we're not done with this thread, and still need destruction.
//  tls_slot.Set(value);
}

int main(int argc, char** argv) {
  printf("[pthread = %lu] main pthread_self = %lu\n", pthread_self());
  tls_slot.Initialize(ThreadLocalStorageCleanup);
  printf("[pthread = %lu] tls_slot.Get() = %p expected nullptr\n", pthread_self(), tls_slot.Get());
  tls_slot.Set(reinterpret_cast<void*>(123));
  int value = reinterpret_cast<intptr_t>(tls_slot.Get());
  printf("[pthread = %lu] tls_slot.Set(123). tls_slot.Get() = %d expected 123\n", pthread_self(), value);

  base::DelegateSimpleThread * simple_thread = new base::DelegateSimpleThread(new ThreadLocalStorageRunner(), "tls thread");
  simple_thread->Start();
  simple_thread->Join();
  printf("[pthread = %lu] simple_thread exit. tls_slot.Get() = %d expected 123\n", pthread_self(), value);
  tls_slot.Free();
  return 0;
}

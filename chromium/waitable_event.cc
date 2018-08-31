#include "base/synchronization/waitable_event.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

class WaitableEventSignaler: public base::PlatformThread::Delegate {
public:
  WaitableEventSignaler(base::WaitableEvent * event): event_(event) {}

  void ThreadMain() override {
      printf("[tid = %ld] sleep(3)\n", pthread_self());
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(3));

      printf("[tid = %ld] event_.Signal()\n", pthread_self());
      event_->Signal();
  }

private:
  base::WaitableEvent * event_;
};

int main(int argc, char ** argv) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL, base::WaitableEvent::InitialState::NOT_SIGNALED);

  printf("event.IsSignaled() = %d\n", event.IsSignaled());

  event.Signal();

  printf("event.Signal(): event.IsSignaled() = %d\n", event.IsSignaled());

  event.Reset();
  printf("event.Reset(): event.IsSignaled() = %d\n", event.IsSignaled());

  printf("event.TimedWait(2000)\n");
  event.TimedWait(base::TimeDelta::FromMilliseconds(2000));
  printf("done\n");
  
  event.Reset();
  WaitableEventSignaler signaler(&event);
  base::PlatformThreadHandle thread;
  base::PlatformThread::Create(0, &signaler, &thread);

  printf("[tid = %ld] event.Wait()\n", pthread_self());
  event.Wait();
  printf("done\n");

//  base::PlatformThread::Join(thread);
  return 0;
}

// sample.cc
#include <iostream>
#include <thread>
#include <xray/xray_log_interface.h>

[[clang::xray_always_instrument]] void f() {
  for (int i = 0; i < 1 << 10; ++i) {
  std::cerr << '.';
  }
}

[[clang::xray_always_instrument]] void g() {
  for (int i = 0; i < 1 << 10; ++i) {
    std::cerr << '-';
  }
  f();
}

uint64_t nbuffers = 0;

int main(int argc, char* argv[]) {
  XRayLogRegisterStatus select_mode_status = __xray_log_select_mode("xray-fdr");
  std::cerr << " select_mode_status = " << select_mode_status;
  if(select_mode_status != XRayLogRegisterStatus::XRAY_REGISTRATION_OK) {
      std::cerr << "__xray_log_select_mode() failed. exit\n";
      exit(1);
  }

  XRayLogInitStatus init_mode_status = __xray_log_init_mode("xray-fdr", "func_duration_threshold_us=5 no_file_flush=0 buffer_size=4096 buffer_max=10");
  std::cerr << " init_mode_status = " << init_mode_status;
  if(init_mode_status != XRayLogInitStatus::XRAY_LOG_INITIALIZED) {
      std::cerr << "__xray_log_init_mode() failed. exit\n";
      exit(1);
  }

  auto patch_status = __xray_patch();
  if(patch_status != XRayPatchingStatus::SUCCESS) {
      std::cerr << "__xray_patch() failed. exit\n";
      exit(1);
  }

  std::cerr << " patch_status = " << patch_status;

  f();
  g();
  f();
  g();
  f();
  f();

  // std::thread t1([] {
  //   for (int i = 0; i < 1 << 10; ++i)
  //     f();
  // });
  // t1.join();

  auto finalize_status = __xray_log_finalize();
  if (finalize_status != XRAY_LOG_FINALIZED) {
      // maybe retry, or bail out.
  }

  auto process_status = __xray_log_process_buffers([](const char * mode, XRayBuffer) {
      ++nbuffers;
  });

  std::cerr << "buffers occupied = " << nbuffers << "\n";

  // At this point, we are sure that the log is finalized, so we may try
  // flushing the log.
  auto flush_status = __xray_log_flushLog();
  if (flush_status != XRAY_LOG_FLUSHED) {
     // maybe retry, or bail out.
  }

  std::thread t2([] {
    g();
  });
  t2.join();
  std::cerr << '\n';

}


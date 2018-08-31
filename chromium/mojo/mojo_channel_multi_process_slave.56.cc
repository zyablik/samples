#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/process_delegate.h"

// inspired by mojo/edk/embedder/embedder_unittest.cc


const char kMojoNamedPipeName[] = "mojo-named-pipe-name";
const char * program;

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

int main(int argc, const char ** argv) {
  program = argv[0];
  printf("%s: main()\n", program);
  base::CommandLine::Init(argc, argv);

  mojo::edk::Init();

  base::Thread io_thread("io thread");
  CHECK(io_thread.StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, io_thread.message_loop()->task_runner());

  mojo::edk::NamedPlatformHandle named_pipe(base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(kMojoNamedPipeName));
  mojo::ScopedMessagePipeHandle master_pipe = mojo::edk::ConnectToPeerProcess(mojo::edk::CreateClientHandle(named_pipe));

  printf("[%s] mojo::edk::ConnectToPeerProcess('%s') master_pipe->value() = %d\n", program, named_pipe.name.c_str(), master_pipe->value());
  
  uint32_t result;

  printf("[%s] before MojoWait\n", program);
  result = MojoWait(master_pipe->value(), MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE, nullptr);
  printf("[%s] after MojoWait result = %d\n", program, result);

  uint32_t num_handles = 0;
  uint32_t message_size = 0;
  result = MojoReadMessage(master_pipe->value(), nullptr, &message_size, nullptr, &num_handles, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("[%s] MojoReadMessage master_pipe result = %d message_size = %d num_handles = %d\n", program, result, message_size, num_handles);

  MojoHandle port;
  std::string message(message_size, 'x');
  result = MojoReadMessage(master_pipe->value(), &message[0], &message_size, &port, &num_handles, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("[%s] MojoReadMessage master_pipe result = %d message = %s port = %d \n", program, result, message.c_str(), port);

  const std::string msg = "world!";
  result = MojoWriteMessage(port, msg.data(), static_cast<uint32_t>(msg.size()), nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  printf("[%s] MojoWriteMessage(port, %s) result = %d\n", program, msg.c_str(), result);
  
  result = MojoClose(port);
  printf("[%s] close port resutl = %d MOJO_RESULT_OK = %d\n", program, result, MOJO_RESULT_OK);

  mojo::edk::ShutdownIPCSupport();

  return 0;
}

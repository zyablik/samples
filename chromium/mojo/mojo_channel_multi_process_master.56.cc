#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/process_delegate.h"

// inspired by mojo/edk/embedder/embedder_unittest.cc

const char kMojoNamedPipeName[] = "mojo-named-pipe-name";

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("DummyProcessDelegate::OnShutdownComplete\n");
  }
};

int main(int argc, const char ** argv) {
  const char * program = argv[0];

  mojo::edk::Init();

  base::Thread io_thread("io thread");
  CHECK(io_thread.StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, io_thread.message_loop()->task_runner());

  base::FilePath temp_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  mojo::edk::NamedPlatformHandle named_pipe = mojo::edk::NamedPlatformHandle(temp_dir.AppendASCII(mojo::edk::GenerateRandomToken()).value());
  base::CommandLine command_line(base::CommandLine::StringVector {"./mojo-channel-multi-process-slave-56"});
  command_line.AppendSwitchNative(kMojoNamedPipeName, named_pipe.name);

  mojo::ScopedMessagePipeHandle slave_pipe = mojo::edk::ConnectToPeerProcess(mojo::edk::CreateServerHandle(named_pipe));
  printf("[%s] ConnectToPeerProcess('%s') slave_pipe->value() = %d\n", program, named_pipe.name.c_str(), slave_pipe->value());

  base::Process slave = base::LaunchProcess(command_line, base::LaunchOptions());

  uint32_t result;

  // basic channel read/write

  MojoHandle server_mp, client_mp;
  MojoCreateMessagePipe(nullptr, &server_mp, &client_mp);

  const std::string kHello = "hello";
  result = MojoWriteMessage(slave_pipe->value(), kHello.data(), static_cast<uint32_t>(kHello.size()), &server_mp, 1, MOJO_WRITE_MESSAGE_FLAG_NONE);
  printf("[%s] MojoWriteMessage result = %d\n", program, result);

  printf("[%s] before MojoWait\n", program);
  result = MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE, nullptr);
  printf("[%s] after MojoWait result = %d\n", program, result);

  uint32_t message_size = 0;
  result = MojoReadMessage(client_mp, nullptr, &message_size, nullptr, nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("[%s] MojoReadMessage: get msg size: result = %d message_size = %u\n", program, result, message_size);

  std::string message(message_size, 'x');
  result = MojoReadMessage(client_mp, &message[0], &message_size, nullptr, nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("[%s] MojoReadMessage get msg content: result = %d message = %s message_size = %d\n", program, result, message.c_str(), message_size);

  mojo::edk::ShutdownIPCSupport();

  return 0;
}

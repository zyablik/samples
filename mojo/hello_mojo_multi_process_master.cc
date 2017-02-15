#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "samples/mojo/hello_mojo.mojom.h"
#include <string>

const char * program;
const char kMojoNamedPipeName[] = "mojo-named-pipe-name";

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

int main(int argc, char ** argv) {
  program = argv[0];
  printf("%s: main()\n", program);

  base::AtExitManager exit_manager;

  mojo::edk::Init();

  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, loop.task_runner());

  base::FilePath temp_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  mojo::edk::NamedPlatformHandle named_pipe = mojo::edk::NamedPlatformHandle(temp_dir.AppendASCII(mojo::edk::GenerateRandomToken()).value());
  base::CommandLine command_line({"./hello-mojo-multi-process-slave"});
  command_line.AppendSwitchNative(kMojoNamedPipeName, named_pipe.name);

  mojo::ScopedMessagePipeHandle slave_pipe = mojo::edk::ConnectToPeerProcess(mojo::edk::CreateServerHandle(named_pipe, false));
  printf("[%s] ConnectToPeerProcess('%s') slave_pipe->value() = %d\n", program, named_pipe.name.c_str(), slave_pipe->value());

  base::Process slave = base::LaunchProcess(command_line, base::LaunchOptions());
  
  mojo::InterfacePtr<examples::HelloMojo> hello_mojo_proxy;
  hello_mojo_proxy.Bind(mojo::InterfacePtrInfo<examples::HelloMojo>(std::move(slave_pipe), 0u));

  std::string request = "hello";
  hello_mojo_proxy->Say(request, base::Bind([](const std::string& response) {
    printf("[%s] hello mojo callback: response = %s\n", program, response.c_str());
  }));

  base::RunLoop().Run();

  mojo::edk::ShutdownIPCSupport();

  return 0;
}

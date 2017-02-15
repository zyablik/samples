#include "base/at_exit.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "samples/mojo/hello_mojo.mojom.h"
#include "services/shell/public/cpp/interface_provider.h"
#include <string>

const char * program;
const char kMojoNamedPipeName[] = "mojo-named-pipe-name";

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

int main(int arc, char ** argv) {
  program = argv[0];
  base::AtExitManager exit_manager;

  mojo::edk::Init();

  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, loop.task_runner());

  base::FilePath temp_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  mojo::edk::NamedPlatformHandle named_pipe = mojo::edk::NamedPlatformHandle(temp_dir.AppendASCII(mojo::edk::GenerateRandomToken()).value());
  base::CommandLine command_line({"./interface-registry-multi-process-slave"});
  command_line.AppendSwitchNative(kMojoNamedPipeName, named_pipe.name);

  mojo::ScopedMessagePipeHandle slave_pipe = mojo::edk::ConnectToPeerProcess(mojo::edk::CreateServerHandle(named_pipe, false));
  printf("[%s] ConnectToPeerProcess('%s') slave_pipe->value() = %d\n", program, named_pipe.name.c_str(), slave_pipe->value());

  base::Process slave = base::LaunchProcess(command_line, base::LaunchOptions());

  shell::mojom::InterfaceProviderPtr remote_interface_provider_ptr;
  remote_interface_provider_ptr.Bind(mojo::InterfacePtrInfo<shell::mojom::InterfaceProvider>(std::move(slave_pipe), 0u));
  shell::InterfaceProvider * remote_interfaces = new shell::InterfaceProvider();
  remote_interfaces->Bind(std::move(remote_interface_provider_ptr));
  
  mojo::InterfacePtr<examples::HelloMojo> hello_mojo_proxy;
  remote_interfaces->GetInterface(&hello_mojo_proxy);

  std::string request = "hello";
  hello_mojo_proxy->Say(request, base::Bind([](const std::string& response) {
    printf("[%s] hello mojo 1 callback: response = %s\n", program, response.c_str());
  }));

  base::RunLoop().Run();

  return 0;
}

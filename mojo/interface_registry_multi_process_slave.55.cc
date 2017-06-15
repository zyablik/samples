#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/hello_mojo.mojom.h"
#include <string>

#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"

const char * program;
const char kMojoNamedPipeName[] = "mojo-named-pipe-name";

class HelloMojoImpl : public examples::HelloMojo {
 public:
  explicit HelloMojoImpl() {
    printf("[%s] HelloMojoImpl::HelloMojoImpl() this = %p\n", program, this);
  }

  ~HelloMojoImpl() override {
    printf("[%s] HelloMojoImpl::~HelloMojoImpl() this = %p\n", program, this);
  }

  // |examples::HelloMojo| implementation:
  void Say(const std::string& request, const SayCallback& callback) override {
    printf("[%s] HelloMojoImpl::Say this = %p request = %s\n", program, this, request.c_str());
    callback.Run((request == "hello") ? "mojo" : "WAT");
  }
};

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

int main(int argc, char ** argv) {
  program = argv[0];

  base::CommandLine::Init(argc, argv);
  printf("%s: main()\n", program);

  base::AtExitManager exit_manager;

  mojo::edk::Init();

  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, loop.task_runner());

  mojo::edk::NamedPlatformHandle named_pipe(base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(kMojoNamedPipeName));
  mojo::ScopedMessagePipeHandle master_pipe = mojo::edk::ConnectToPeerProcess(mojo::edk::CreateClientHandle(named_pipe));

  printf("[%s] mojo::edk::ConnectToPeerProcess('%s') master_pipe->value() = %d\n", program, named_pipe.name.c_str(), master_pipe->value());

  mojo::InterfaceRequest<shell::mojom::InterfaceProvider> interface_provider_request;
  interface_provider_request.Bind(std::move(master_pipe));

  shell::InterfaceRegistry * interface_registry = new shell::InterfaceRegistry();
  interface_registry->AddInterface(base::Bind(
    [](examples::HelloMojoRequest request){ mojo::MakeStrongBinding(base::MakeUnique<HelloMojoImpl>(), std::move(request)); }
  ));

  interface_registry->Bind(std::move(interface_provider_request));

  base::RunLoop().Run();

  return 0;
}

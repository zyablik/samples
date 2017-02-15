#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/service_manager_dummy_runner_multi_process/some_interface.mojom.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/service_manager.h"
#include <string>
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/common/switches.h"


const char * program;

class OtherService: public shell::Service,
                    public shell::InterfaceFactory<mojom::SomeInterface>,
                    public mojom::SomeInterface
{
public:
  OtherService() {
    printf("[%s]: OtherService::OtherService this = %p\n", program, this);
  }

  void OnStart(const shell::Identity& identity) override {
    printf("[%s]: OtherService::OnStart this = %p\n", program, this);
  }

  // Overridden from shell::Service:
  bool OnConnect(const shell::Identity& remote_identity, shell::InterfaceRegistry * registry) override {
    printf("[%s]: OtherService::OnConnect this = %p remote_identity.name = %s\n", program, this, remote_identity.name().c_str());
    registry->AddInterface<mojom::SomeInterface>(this);
    return true;
  }

  // Overridden from shell::InterfaceFactory<mojom::SomeInterface>:
  void Create(const shell::Identity& remote_identity, mojom::SomeInterfaceRequest request) override {
    printf("[%s] OtherService::Create this = %p\n", program, this);
    bindings_.AddBinding(this, std::move(request));
  }

  // Overridden from mojom::SomeInterface:
  void Foo() override { 
    printf("[%s] OtherService::Foo this = %p\n", program, this);
  }

  mojo::BindingSet<mojom::SomeInterface> bindings_;
};

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

int main(int argc, char ** argv) {
  program = argv[0];

  printf("[%s] cmd line = ", program);
  for(int i = 0; i < argc; i++) {
    printf("%s ", argv[i]);
  }
  printf("\n");

  base::CommandLine::Init(argc, argv);
  printf("%s: main()\n", program);

  base::AtExitManager exit_manager;

  mojo::edk::Init();

  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, loop.task_runner());

  mojo::edk::SetParentPipeHandleFromCommandLine();
  shell::mojom::ServiceRequest other_service_request = shell::GetServiceRequestFromCommandLine();

  shell::Service * other_service = new OtherService();
  std::unique_ptr<shell::ServiceContext> context(new shell::ServiceContext(other_service, std::move(other_service_request)));
  other_service->set_context(std::move(context)); // from shell::ServiceRunner(new MyService).Run(service_request_handle)

  base::RunLoop().Run();
}

















#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/service_manager_native_runner_single_process/some_interface.mojom.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/service_manager.h"
#include <string>

class OtherService: public shell::Service,
                    public shell::InterfaceFactory<mojom::SomeInterface>,
                    public mojom::SomeInterface
{
public:
  OtherService() {
    printf("OtherService::OtherService this = %p\n", this);
  }

  void OnStart(const shell::Identity& identity) override {
    printf("OtherService::OnStart this = %p\n", this);
  }

  // Overridden from shell::Service:
  bool OnConnect(const shell::Identity& remote_identity, shell::InterfaceRegistry * registry) override {
    printf("OtherService::OnConnect this = %p remote_identity.name = %s\n", this, remote_identity.name().c_str());
    registry->AddInterface<mojom::SomeInterface>(this);
    return true;
  }

  // Overridden from shell::InterfaceFactory<mojom::SomeInterface>:
  void Create(const shell::Identity& remote_identity, mojom::SomeInterfaceRequest request) override {
    printf("OtherService::Create this = %p\n", this);
    bindings_.AddBinding(this, std::move(request));
  }

  // Overridden from mojom::SomeInterface:
  void Foo() override {
    printf("[pid = %d] OtherService::Foo this = %p\n", getpid(), this);
  }

  mojo::BindingSet<mojom::SomeInterface> bindings_;
};

extern "C" MojoResult ServiceMain(MojoHandle service_request_handle) __attribute__ ((visibility ("default")))  {
  return shell::ServiceRunner(new OtherService).Run(service_request_handle);
}

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/hello_mojo.mojom.h"
#include <string>

#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/cpp/interface_provider.h"

class HelloMojoImpl : public examples::HelloMojo {
 public:
  
  explicit HelloMojoImpl() {
    printf("HelloMojoImpl::HelloMojoImpl() this = %p\n", this);
  }

  ~HelloMojoImpl() override {
    printf("HelloMojoImpl::~HelloMojoImpl() this = %p\n", this);
  }

  // |examples::HelloMojo| implementation:
  void Say(const std::string& request, const SayCallback& callback) override {
    printf("HelloMojoImpl::Say this = %p request = %s\n", this, request.c_str());
    callback.Run((request == "hello") ? "mojo" : "WAT");
  }
};
 
int main(int arc, char ** argv) {
  base::AtExitManager exit_manager;
  base::MessageLoop loop;
  base::RunLoop run_loop;
  mojo::edk::Init();

  std::string request = "hello";

  shell::InterfaceRegistry * interface_registry = new shell::InterfaceRegistry();

  interface_registry->AddInterface(base::Bind(
    [](examples::HelloMojoRequest request){ mojo::MakeStrongBinding(base::MakeUnique<HelloMojoImpl>(), std::move(request)); }
  ));

//  shell::mojom::InterfaceProviderPtr interface_provider_ptr;
//  shell::mojom::InterfaceProviderRequest interface_provider_request = mojo::GetProxy(&interface_provider_ptr);

  // exanded mojo::GetProxy:
  mojo::MessagePipe pipe;
  mojo::InterfaceRequest<shell::mojom::InterfaceProvider> interface_provider_request;
  interface_provider_request.Bind(std::move(pipe.handle1));

  mojo::InterfacePtr<shell::mojom::InterfaceProvider> interface_provider_ptr;
  interface_provider_ptr.Bind(mojo::InterfacePtrInfo<shell::mojom::InterfaceProvider>(std::move(pipe.handle0), 0u), base::ThreadTaskRunnerHandle::Get());

  interface_registry->Bind(std::move(interface_provider_request));

  // can call GetInterface on interface_provider_ptr directly
//  examples::HelloMojoPtr hello_mojo_ptr_1;
//  interface_provider_ptr->GetInterface(examples::HelloMojo::Name_, mojo::GetProxy(&hello_mojo_ptr_1).PassMessagePipe());

//  hello_mojo_ptr_1->Say(request, base::Bind([](const std::string& response) {
//    printf("hello mojo 1 callback: response = %s\n", response.c_str());
//  }));

  // or via InterfaceProvider:
  shell::InterfaceProvider * interface_provider = new shell::InterfaceProvider();
  interface_provider->Bind(std::move(interface_provider_ptr));

  examples::HelloMojoPtr hello_mojo_ptr_2;
  interface_provider->GetInterface(&hello_mojo_ptr_2);

  hello_mojo_ptr_2->Say(request, base::Bind([](const std::string& response) {
    printf("hello mojo 2 callback: response = %s\n", response.c_str());
  }));

  base::RunLoop().Run();

  return 0;
}

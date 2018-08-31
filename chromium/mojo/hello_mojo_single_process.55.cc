#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/hello_mojo.mojom.h"
#include <string>

// inspired by mojo/src/examples/hello_mojo and chromium/src/mojo/public/cpp/bindings/tests/binding_unittest.cc

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

  mojo::InterfacePtr<examples::HelloMojo> hello_mojo_proxy;
  mojo::InterfaceRequest<examples::HelloMojo> hello_mojo_imp_request = mojo::GetProxy(&hello_mojo_proxy);

  HelloMojoImpl hello_mojo_imp;
  mojo::Binding<examples::HelloMojo> binding(&hello_mojo_imp, std::move(hello_mojo_imp_request));
//  auto binding = MakeStrongBinding(base::MakeUnique<HelloMojoImpl>(&hello_mojo_imp), std::move(hello_mojo_imp_request));
  
  std::string request = "hello";
  hello_mojo_proxy->Say(request, base::Bind([](const std::string& response) {
    printf("hello mojo 1 callback: response = %s\n", response.c_str());
  }));

  base::RunLoop().RunUntilIdle();

  MojoHandle server_mp, client_mp;
  uint32_t result = MojoCreateMessagePipe(nullptr, &server_mp, &client_mp);
  printf("MojoCreateMessagePipe result = %d\n", result);

  mojo::InterfacePtr<examples::HelloMojo> hello_mojo_proxy_2;
  hello_mojo_proxy_2.Bind(mojo::InterfacePtrInfo<examples::HelloMojo>(mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(client_mp)), 0u));

  mojo::InterfaceRequest<examples::HelloMojo> hello_mojo_imp_request_2;
  hello_mojo_imp_request_2.Bind(mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(server_mp)));

  mojo::Binding<examples::HelloMojo> binding2(&hello_mojo_imp, std::move(hello_mojo_imp_request_2));

  hello_mojo_proxy_2->Say(request, base::Bind([](const std::string& response) {
    printf("hello mojo 2 callback: response = %s\n", response.c_str());
  }));

  base::RunLoop().RunUntilIdle();
//  run_loop.Run();

  return 0;
}

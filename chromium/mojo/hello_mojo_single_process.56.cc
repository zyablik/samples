#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/hello_mojo.mojom.h"
#include <string>

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

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("DummyProcessDelegate::OnShutdownComplete\n");
  }
};

int hello_mojo_main(int arc, char ** argv) {
  base::AtExitManager exit_manager;
  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  base::RunLoop run_loop;
  mojo::edk::Init();

  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, loop.task_runner());
  
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
  
  std::string child_token = mojo::edk::GenerateRandomToken();
  std::string channel_id = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle host_handle = mojo::edk::CreateParentMessagePipe(channel_id, child_token);
  mojo::ScopedMessagePipeHandle child_handle = mojo::edk::CreateChildMessagePipe(child_token);
  printf("host_handle = %d child_handle = %d\n", host_handle->value(), child_handle->value());
  mojo::InterfacePtr<examples::HelloMojo> hello_mojo_proxy_2;
//  hello_mojo_proxy_2.Bind(mojo::InterfacePtrInfo<examples::HelloMojo>(mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(client_mp)), 0u));
  hello_mojo_proxy_2.Bind(mojo::InterfacePtrInfo<examples::HelloMojo>(std::move(host_handle), 0u));

  mojo::InterfaceRequest<examples::HelloMojo> hello_mojo_imp_request_2;
//  hello_mojo_imp_request_2.Bind(mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(server_mp)));
  hello_mojo_imp_request_2.Bind(std::move(child_handle));

  mojo::Binding<examples::HelloMojo> binding2(&hello_mojo_imp, std::move(hello_mojo_imp_request_2));

  hello_mojo_proxy_2->Say(request, base::Bind([](const std::string& response) {
    printf("hello mojo 2 callback: response = %s\n", response.c_str());
  }));

  base::RunLoop().RunUntilIdle();
//  run_loop.Run();

  return 0;
}

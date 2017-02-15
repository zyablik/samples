#include "base/at_exit.h"
#include "base/json/json_reader.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/service_manager_native_runner_single_process/some_interface.mojom.h"
#include "services/catalog/catalog.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/store.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/runner/host/in_process_native_runner.h"
#include "services/shell/service_manager.h"
#include <string>

// from service_manager_context
class BuiltinManifestProvider : public catalog::ManifestProvider {
 public:
  BuiltinManifestProvider() {}
  ~BuiltinManifestProvider() override {}

  void AddManifestValue(const std::string& name, std::unique_ptr<base::Value> manifest_contents) {
    auto result = manifests_.insert(std::make_pair(name, std::move(manifest_contents)));
    DCHECK(result.second) << "Duplicate manifest entry: " << name;
  }

 private:
  // catalog::ManifestProvider:
  std::unique_ptr<base::Value> GetManifest(const std::string& name) override {
    auto it = manifests_.find(name);
    return it != manifests_.end() ? it->second->CreateDeepCopy() : nullptr;
  }

  std::map<std::string, std::unique_ptr<base::Value>> manifests_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinManifestProvider);
};

int main(int arc, char ** argv) {
  printf("[pid = %d] main()\n", getpid());
  base::CommandLine::Init(arc, argv);
  base::AtExitManager exit_manager;
  base::MessageLoop loop;
  base::RunLoop run_loop;
  mojo::edk::Init();
  base::i18n::InitializeICU();

  // inspired by ServiceManagerContext::ServiceManagerContext()
  BuiltinManifestProvider manifest_provider;

  std::unique_ptr<shell::NativeRunnerFactory> native_runner_factory(new shell::InProcessNativeRunnerFactory(loop.task_runner().get()));
  catalog::Catalog catalog(loop.task_runner().get(), nullptr, &manifest_provider);
  shell::ServiceManager * service_manager = new shell::ServiceManager(std::move(native_runner_factory), catalog.TakeService());

  std::unique_ptr<shell::ConnectParams> connect_params(new shell::ConnectParams());
  connect_params->set_source(shell::Identity("service:root", shell::mojom::kRootUserID));
  connect_params->set_target(shell::Identity("service:other_service", shell::mojom::kRootUserID));

  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderRequest remote_request = mojo::GetProxy(&remote_interfaces);
  connect_params->set_remote_interfaces(std::move(remote_request));

  service_manager->Connect(std::move(connect_params));
  
  mojom::SomeInterfacePtr some_interface_ptr;
  remote_interfaces->GetInterface(mojom::SomeInterface::Name_, mojo::GetProxy(&some_interface_ptr).PassMessagePipe());

  some_interface_ptr->Foo();

  base::RunLoop().Run();
}

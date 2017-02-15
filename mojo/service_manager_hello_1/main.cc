#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/service_manager_hello/some_interface.mojom.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/service_manager.h"
#include <string>

class OtherService: public shell::Service,
                    public shell::InterfaceFactory<mojom::SomeInterface>,
                    public mojom::SomeInterface
{
public:
  void OnStart(const shell::Identity& identity) override {
    printf("OtherService::OnStart\n");
  }
  
  // Overridden from shell::Service:
  bool OnConnect(const shell::Identity& remote_identity, shell::InterfaceRegistry* registry) override {
    printf("OtherService::OnConnect");
    registry->AddInterface<mojom::SomeInterface>(this);
    return true;
  }

  // Overridden from shell::InterfaceFactory<mojom::SomeInterface>:
  void Create(const shell::Identity& remote_identity, mojom::SomeInterfaceRequest request) override {
    printf("OtherService::Create\n");
    bindings_.AddBinding(this, std::move(request));
  }

  // Overridden from mojom::SomeInterface:
  void Foo() override { 
    printf("OtherService::Foo\n");
  }

  mojo::BindingSet<mojom::SomeInterface> bindings_;
};


class MyService : public shell::Service {
 public:
  MyService() {}
  ~MyService() override {}

  void OnStart(const shell::Identity& identity) override {
    printf("MyService::OnStart identity name = %s user_id = %s instance = %s\n", identity.name().c_str(), identity.user_id().c_str(), identity.instance().c_str());
    std::unique_ptr<shell::Connection> connection = context()->connector()->Connect("service:other_service");
    mojom::SomeInterfacePtr some_interface;
    connection->GetInterface(&some_interface);
    some_interface->Foo();
  }

  bool OnConnect(const shell::Identity& remote_identity, shell::InterfaceRegistry* registry) override {
    printf("MyService::OnConnect\n");
    return true;
  }
  
  bool OnStop() override {
    printf("MyService::OnStop\n");
    return true;
  }
  
};

MojoResult ServiceMain(MojoHandle service_request_handle) {
  return shell::ServiceRunner(new MyService).Run(service_request_handle);
}

class DummyCatalog: public shell::mojom::Service,
                    public shell::mojom::Resolver,
                    public shell::InterfaceFactory<shell::mojom::Resolver>,
                    public catalog::mojom::Catalog,
                    public shell::InterfaceFactory<catalog::mojom::Catalog>
{
 public:
  DummyCatalog() {
    printf("DummyCatalog::DummyCatalog() this = %p\n", this);
  }

  ~DummyCatalog() override {
    printf("DummyCatalog::~DummyCatalog() this = %p\n", this);
  }

  void OnStart(const shell::Identity& identity, const OnStartCallback& callback) override {
    printf("DummyCatalog::OnStart identity name = %s user_id = %s instance = %s\n", identity.name().c_str(), identity.user_id().c_str(), identity.instance().c_str());
    callback.Run(nullptr);
  }

  void OnConnect(const shell::Identity& source, shell::mojom::InterfaceProviderRequest interfaces, const shell::CapabilityRequest& allowed_capabilities) override {
    printf("DummyCatalog::OnConnect source name = %s user_id = %s instance = %s\n", source.name().c_str(), source.user_id().c_str(), source.instance().c_str());
    shell::InterfaceRegistry * registry = new shell::InterfaceRegistry(source, allowed_capabilities);
    registry->Bind(std::move(interfaces));
    registry->AddInterface<shell::mojom::Resolver>(this);
    registry->AddInterface<catalog::mojom::Catalog>(this);

//    registry->AddInterface<filesystem::mojom::Directory>(this);

  }

  // shell::InterfaceFactory<shell::mojom::Resolver>:
  void Create(const shell::Identity& remote_identity, shell::mojom::ResolverRequest request) override {
     printf("DummyCatalog::Create(ResolverRequest) remote_identity name = %s user_id = %s instance = %s\n", remote_identity.name().c_str(), remote_identity.user_id().c_str(), remote_identity.instance().c_str());
     mojo::MakeStrongBinding(std::unique_ptr<DummyCatalog>(this), std::move(request));
  }

  // shell::InterfaceFactory<shell::mojom::Catalog>:
  void Create(const shell::Identity& remote_identity, catalog::mojom::CatalogRequest request) override {
     printf("DummyCatalog::Create(CatalogRequest) remote_identity name = %s user_id = %s instance = %s\n", remote_identity.name().c_str(), remote_identity.user_id().c_str(), remote_identity.instance().c_str());
     mojo::MakeStrongBinding(std::unique_ptr<DummyCatalog>(this), std::move(request));
  }

  // shell::InterfaceFactory<shell::mojom::Resolver>:
  void ResolveMojoName(const std::string& mojo_name, const ResolveMojoNameCallback& callback) {
    printf("DummyCatalog::ResolveMojoName mojo_name = %s\n", mojo_name.c_str());
    shell::mojom::ResolveResultPtr result(shell::mojom::ResolveResult::New());
    result->name = mojo_name;
    result->resolved_name = mojo_name;
    result->qualifier = "qualifier";

    if(mojo_name == "service:my_service") {
        shell::CapabilitySpec spec;
        shell::CapabilityRequest request;
        spec.required["service:other_service"] = request;
    } else if(mojo_name == "service:other_service") {
      shell::CapabilitySpec spec;
//      spec.provided["service:other_service"] = {"mojom.SomeInterface"};
      result->capabilities = spec;
    }

    result->package_path = base::FilePath(std::string("file:///dummy/path/").append(mojo_name));
    callback.Run(std::move(result));
  }

  // mojom::Catalog:
  void GetEntries(const base::Optional<std::vector<std::string>>& names, const GetEntriesCallback& callback) override {
    printf("DummyCatalog::GetEntries\n");
    std::vector<catalog::mojom::EntryPtr> entries;
    callback.Run(std::move(entries));
  }

  void GetEntriesProvidingClass(const std::string& clazz, const GetEntriesProvidingClassCallback& callback) override {
    printf("DummyCatalog::GetEntriesProvidingClass clazz = %s\n", clazz.c_str());
    std::vector<catalog::mojom::EntryPtr> entries;
    callback.Run(std::move(entries));
  }

  void GetEntriesConsumingMIMEType(const std::string& mime_type, const GetEntriesConsumingMIMETypeCallback& callback) override {
    printf("DummyCatalog::GetEntriesConsumingMIMEType\n");
  }

  void GetEntriesSupportingScheme(const std::string& scheme, const GetEntriesSupportingSchemeCallback& callback) override {
    printf("DummyCatalog::GetEntriesSupportingScheme\n");
  }
};

// inspired by InProcessNativeRunner::Start and InProcessNativeRunner::Run
class DummyNativeRunner: public shell::NativeRunner
{
public:
  shell::mojom::ServicePtr Start(const base::FilePath& app_path, const shell::Identity& target, bool start_sandboxed,
      const base::Callback<void(base::ProcessId)>& pid_available_callback,
      const base::Closure& app_completed_callback) override
  {
    printf("DummyNativeRunner::Start app_path = %s target = { name: '%s', user_id: '%s', instance: '%s' }\n", app_path.MaybeAsASCII().c_str(), target.name().c_str(), target.user_id().c_str(), target.instance().c_str());
    shell::mojom::ServicePtr client;
    shell::mojom::ServiceRequest request = mojo::GetProxy(&client);

    // inspired by ServiceRunner::Run()
    shell::Service * service = new OtherService();
    std::unique_ptr<shell::ServiceContext> context(new shell::ServiceContext(service, mojo::MakeRequest<shell::mojom::Service>(request.PassMessagePipe())));
    service->set_context(std::move(context));

    pid_available_callback.Run(base::Process::Current().Pid());
//    app_completed_callback.Run();

    return client;
  }
};

class DummyRunnerFactory: public shell::NativeRunnerFactory
{
public:
  std::unique_ptr<shell::NativeRunner> Create(const base::FilePath& app_path) override {
    printf("DummyRunnerFactory::Create app_path = %s\n", app_path.MaybeAsASCII().c_str());
    return std::unique_ptr<shell::NativeRunner>(new DummyNativeRunner());
  }
};

int main(int arc, char ** argv) {
  base::CommandLine::Init(arc, argv);
  base::AtExitManager exit_manager;
  base::MessageLoop loop;
  base::RunLoop run_loop;
  mojo::edk::Init();

  std::unique_ptr<shell::NativeRunnerFactory> runner_factory(new DummyRunnerFactory());
  shell::mojom::ServicePtr catalog_ptr;
  shell::mojom::ServiceRequest catalog_request = mojo::GetProxy(&catalog_ptr);
  
  DummyCatalog catalog;
  mojo::Binding<shell::mojom::Service> catalog_binding(&catalog, std::move(catalog_request));
  
  shell::ServiceManager * service_manager = new shell::ServiceManager(std::move(runner_factory), std::move(catalog_ptr));
  shell::mojom::ServiceRequest my_service_request = service_manager->StartEmbedderService("service:my_service");

  shell::Service * my_service = new MyService();
  std::unique_ptr<shell::ServiceContext> context(new shell::ServiceContext(my_service, mojo::MakeRequest<shell::mojom::Service>(my_service_request.PassMessagePipe())));
  my_service->set_context(std::move(context)); // from shell::ServiceRunner(new MyService).Run(service_request_handle)
  
  base::RunLoop().Run();
}

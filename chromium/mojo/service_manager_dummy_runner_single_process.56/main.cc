#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/service_manager_dummy_runner_single_process/some_interface.mojom.h"
#include "services/service_manager/native_runner.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/service_manager.h"
#include <string>

class OtherService: public service_manager::Service,
                    public service_manager::InterfaceFactory<mojom::SomeInterface>,
                    public mojom::SomeInterface
{
public:
  OtherService() {
    printf("OtherService::OtherService this = %p\n", this);
  }

  void OnStart() override {
    printf("OtherService::OnStart this = %p\n", this);
  }

  // Overridden from service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info, service_manager::InterfaceRegistry * registry) override {
    printf("OtherService::OnConnect this = %p remote_info = %s\n", this, remote_info.identity.name().c_str());
    registry->AddInterface<mojom::SomeInterface>(this);
    return true;
  }

  // Overridden from service_manager::InterfaceFactory<mojom::SomeInterface>:
  void Create(const service_manager::Identity& remote_identity, mojom::SomeInterfaceRequest request) override {
    printf("OtherService::Create this = %p\n", this);
    bindings_.AddBinding(this, std::move(request));
  }

  // Overridden from mojom::SomeInterface:
  void Foo() override { 
    printf("OtherService::Foo this = %p\n", this);
  }

  mojo::BindingSet<mojom::SomeInterface> bindings_;
};

class MyService : public service_manager::Service {
 public:
  MyService() {}
  ~MyService() override {}

  void OnStart() override {
    printf("MyService::OnStart\n");
    std::unique_ptr<service_manager::Connection> connection = context()->connector()->Connect("service:other_service");
    mojom::SomeInterfacePtr some_interface;
    connection->GetInterface(&some_interface);
    some_interface->Foo();
  }

  bool OnConnect(const service_manager::ServiceInfo& remote_info, service_manager::InterfaceRegistry* registry) override {
    printf("MyService::OnConnect\n");
    return true;
  }
  
  bool OnStop() override {
    printf("MyService::OnStop\n");
    return true;
  }
  
};

class DummyResolver: public service_manager::mojom::Service,
                    public service_manager::mojom::Resolver,
                    public service_manager::InterfaceFactory<service_manager::mojom::Resolver>
{
 public:
  DummyResolver() {
    printf("DummyResolver::DummyResolver() this = %p\n", this);
  }

  ~DummyResolver() override {
    printf("DummyResolver::~DummyResolver() this = %p\n", this);
  }

  void OnStart(const service_manager::ServiceInfo& info, const OnStartCallback& callback) override {
    printf("DummyResolver::OnStart identity name = %s user_id = %s instance = %s\n", info.identity.name().c_str(), info.identity.user_id().c_str(), info.identity.instance().c_str());
    callback.Run(nullptr, nullptr);
  }

  void OnConnect(const service_manager::ServiceInfo& source_info, service_manager::mojom::InterfaceProviderRequest interfaces, const OnConnectCallback& callback) override {
    printf("DummyResolver::OnConnect source name = %s user_id = %s instance = %s\n", source_info.identity.name().c_str(), source_info.identity.user_id().c_str(), source_info.identity.instance().c_str());
    service_manager::InterfaceRegistry * registry = new service_manager::InterfaceRegistry(source_info.identity.name());
    registry->Bind(std::move(interfaces), service_manager::Identity(), service_manager::InterfaceProviderSpec(), service_manager::Identity(), service_manager::InterfaceProviderSpec());
    registry->AddInterface<service_manager::mojom::Resolver>(this);
    callback.Run();
  }

  // service_manager::InterfaceFactory<service_manager::mojom::Resolver>:
  void Create(const service_manager::Identity& remote_identity, service_manager::mojom::ResolverRequest request) override {
     printf("DummyResolver::Create(ResolverRequest) remote_identity name = %s user_id = %s instance = %s\n", remote_identity.name().c_str(), remote_identity.user_id().c_str(), remote_identity.instance().c_str());
     mojo::MakeStrongBinding(std::unique_ptr<DummyResolver>(this), std::move(request));
  }

  // service_manager::InterfaceFactory<service_manager::mojom::Resolver>:
  void ResolveMojoName(const std::string& mojo_name, const ResolveMojoNameCallback& callback) {
    printf("DummyResolver::ResolveMojoName mojo_name = %s\n", mojo_name.c_str());
    service_manager::mojom::ResolveResultPtr result(service_manager::mojom::ResolveResult::New());
    result->name = mojo_name;
    result->resolved_name = mojo_name;
    result->qualifier = "qualifier";

    service_manager::InterfaceProviderSpecMap specs;
    service_manager::InterfaceProviderSpec spec;
    if(mojo_name == "service:my_service") {
       spec.requires["*"] = {"other_service"};
    } else if(mojo_name == "service:other_service") {
      spec.provides["other_service"] = {"mojom::SomeInterface"};
    }
    specs[service_manager::mojom::kServiceManager_ConnectorSpec] = spec;
    result->interface_provider_specs = specs;

    result->package_path = base::FilePath(std::string("file:///dummy/path/").append(mojo_name));
    callback.Run(std::move(result));
  }
};

// inspired by InProcessNativeRunner::Start and InProcessNativeRunner::Run
class DummyNativeRunner: public service_manager::NativeRunner
{
public:
  service_manager::mojom::ServicePtr Start(const base::FilePath& app_path, const service_manager::Identity& target, bool start_sandboxed,
      const base::Callback<void(base::ProcessId)>& pid_available_callback,
      const base::Closure& app_completed_callback) override
  {
    printf("DummyNativeRunner::Start app_path = %s target = { name: '%s', user_id: '%s', instance: '%s' }\n", app_path.MaybeAsASCII().c_str(), target.name().c_str(), target.user_id().c_str(), target.instance().c_str());
    service_manager::mojom::ServicePtr client;
    service_manager::mojom::ServiceRequest request = mojo::GetProxy(&client);

    if(target.name() == "service:other_service") {
      // inspired by ServiceRunner::Run()
      std::unique_ptr<service_manager::Service> service(new OtherService());
      new service_manager::ServiceContext(std::move(service), mojo::MakeRequest<service_manager::mojom::Service>(request.PassMessagePipe()));
    } else {
      printf("DummyNativeRunner::Start unknown target.name() = %s\n", target.name().c_str());
      exit(1);
    }

    pid_available_callback.Run(base::Process::Current().Pid());

    return client;
  }
};

class DummyRunnerFactory: public service_manager::NativeRunnerFactory
{
public:
  std::unique_ptr<service_manager::NativeRunner> Create(const base::FilePath& app_path) override {
    printf("DummyRunnerFactory::Create app_path = %s\n", app_path.MaybeAsASCII().c_str());
    return std::unique_ptr<service_manager::NativeRunner>(new DummyNativeRunner());
  }
};

int main(int arc, char ** argv) {
  base::CommandLine::Init(arc, argv);
  base::AtExitManager exit_manager;
  base::MessageLoop loop;
  base::RunLoop run_loop;
  mojo::edk::Init();

  std::unique_ptr<service_manager::NativeRunnerFactory> runner_factory(new DummyRunnerFactory());
  service_manager::mojom::ServicePtr resolver_ptr;
  service_manager::mojom::ServiceRequest resolver_request = mojo::GetProxy(&resolver_ptr);
  
  DummyResolver resolver;
  mojo::Binding<service_manager::mojom::Service> catalog_binding(&resolver, std::move(resolver_request));
  
  service_manager::ServiceManager * service_manager = new service_manager::ServiceManager(std::move(runner_factory), std::move(resolver_ptr));
  service_manager::mojom::ServiceRequest my_service_request = service_manager->StartEmbedderService("service:my_service");

  // from service_manager::ServiceRunner
  std::unique_ptr<service_manager::Service> my_service(new MyService());
  new service_manager::ServiceContext(std::move(my_service), std::move(my_service_request));

  
  std::unique_ptr<service_manager::ConnectParams> connect_params(new service_manager::ConnectParams());
  connect_params->set_source(service_manager::Identity("service:my_service", service_manager::mojom::kRootUserID));
  connect_params->set_target(service_manager::Identity("service:other_service", service_manager::mojom::kRootUserID));

  service_manager::mojom::InterfaceProviderPtr remote_interfaces;
  service_manager::mojom::InterfaceProviderRequest remote_request = mojo::GetProxy(&remote_interfaces);
  connect_params->set_remote_interfaces(std::move(remote_request));

  service_manager->Connect(std::move(connect_params));
  
  mojom::SomeInterfacePtr some_interface_ptr;
  remote_interfaces->GetInterface(mojom::SomeInterface::Name_, mojo::GetProxy(&some_interface_ptr).PassMessagePipe());

  some_interface_ptr->Foo();

  base::RunLoop().Run();
}

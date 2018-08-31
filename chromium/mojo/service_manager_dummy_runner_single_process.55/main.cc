#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/service_manager_dummy_runner_single_process/some_interface.mojom.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
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
    printf("OtherService::Foo this = %p\n", this);
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

class DummyResolver: public shell::mojom::Service,
                    public shell::mojom::Resolver,
                    public shell::InterfaceFactory<shell::mojom::Resolver>
{
 public:
  DummyResolver() {
    printf("DummyResolver::DummyResolver() this = %p\n", this);
  }

  ~DummyResolver() override {
    printf("DummyResolver::~DummyResolver() this = %p\n", this);
  }

  void OnStart(const shell::Identity& identity, const OnStartCallback& callback) override {
    printf("DummyResolver::OnStart identity name = %s user_id = %s instance = %s\n", identity.name().c_str(), identity.user_id().c_str(), identity.instance().c_str());
    callback.Run(nullptr);
  }

  void OnConnect(const shell::Identity& source, shell::mojom::InterfaceProviderRequest interfaces, const shell::CapabilityRequest& allowed_capabilities) override {
    printf("DummyResolver::OnConnect source name = %s user_id = %s instance = %s\n", source.name().c_str(), source.user_id().c_str(), source.instance().c_str());
    shell::InterfaceRegistry * registry = new shell::InterfaceRegistry(source, allowed_capabilities);
    registry->Bind(std::move(interfaces));
    registry->AddInterface<shell::mojom::Resolver>(this);
  }

  // shell::InterfaceFactory<shell::mojom::Resolver>:
  void Create(const shell::Identity& remote_identity, shell::mojom::ResolverRequest request) override {
     printf("DummyResolver::Create(ResolverRequest) remote_identity name = %s user_id = %s instance = %s\n", remote_identity.name().c_str(), remote_identity.user_id().c_str(), remote_identity.instance().c_str());
     mojo::MakeStrongBinding(std::unique_ptr<DummyResolver>(this), std::move(request));
  }

  // shell::InterfaceFactory<shell::mojom::Resolver>:
  void ResolveMojoName(const std::string& mojo_name, const ResolveMojoNameCallback& callback) {
    printf("DummyResolver::ResolveMojoName mojo_name = %s\n", mojo_name.c_str());
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

    if(target.name() == "service:other_service") {
      // inspired by ServiceRunner::Run()
      shell::Service * service = new OtherService();
      std::unique_ptr<shell::ServiceContext> context(new shell::ServiceContext(service, mojo::MakeRequest<shell::mojom::Service>(request.PassMessagePipe())));
      service->set_context(std::move(context));
    } else {
      printf("DummyNativeRunner::Start unknown target.name() = %s\n", target.name().c_str());
      exit(1);
    }

    pid_available_callback.Run(base::Process::Current().Pid());

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
  shell::mojom::ServicePtr resolver_ptr;
  shell::mojom::ServiceRequest resolver_request = mojo::GetProxy(&resolver_ptr);
  
  DummyResolver resolver;
  mojo::Binding<shell::mojom::Service> catalog_binding(&resolver, std::move(resolver_request));
  
  shell::ServiceManager * service_manager = new shell::ServiceManager(std::move(runner_factory), std::move(resolver_ptr));
  shell::mojom::ServiceRequest my_service_request = service_manager->StartEmbedderService("service:my_service");

  shell::Service * my_service = new MyService();
  std::unique_ptr<shell::ServiceContext> context(new shell::ServiceContext(my_service, std::move(my_service_request)));
  my_service->set_context(std::move(context)); // from shell::ServiceRunner(new MyService).Run(service_request_handle)

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

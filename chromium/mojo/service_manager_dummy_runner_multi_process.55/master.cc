#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "samples/mojo/service_manager_dummy_runner_multi_process/some_interface.mojom.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/service_manager.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/common/switches.h"
#include <string>

const char * program;

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

    if(mojo_name == "service:other_service") {
      shell::CapabilitySpec spec;
//      spec.provided["service:other_service"] = {"mojom.SomeInterface"};
      result->capabilities = spec;
      result->package_path =  base::FilePath("./service-manager-dummy-runner-multi-process-slave");
    } else {
      result->package_path = base::FilePath(std::string("file:///dummy/path/").append(mojo_name));
    }

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

    base::CommandLine command_line({app_path.value()});
    const std::string child_token = mojo::edk::GenerateRandomToken();
    shell::mojom::ServicePtr service = shell::PassServiceRequestOnCommandLine(&command_line, child_token);
  
    mojo::edk::HandlePassingInformation handle_passing_info;
    mojo::edk::PlatformChannelPair mojo_ipc_channel;
    mojo_ipc_channel.PrepareToPassClientHandleToChildProcess(&command_line, &handle_passing_info);

    base::LaunchOptions options;
    options.fds_to_remap = &handle_passing_info;

    base::Process slave = base::LaunchProcess(command_line, options);

    mojo::edk::ChildProcessLaunched(
      slave.Handle(),
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(mojo_ipc_channel.PassServerHandle().release().handle)),
      child_token
    );

    pid_available_callback.Run(slave.Pid());

    return service;
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

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

int main(int argc, char ** argv) {
  program = argv[0];
  printf("%s: main()\n", program);

  base::AtExitManager exit_manager;

  mojo::edk::Init();

  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  DummyProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, loop.task_runner());

  std::unique_ptr<shell::NativeRunnerFactory> runner_factory(new DummyRunnerFactory());
  shell::mojom::ServicePtr resolver_ptr;
  shell::mojom::ServiceRequest resolver_request = mojo::GetProxy(&resolver_ptr);

  DummyResolver resolver;
  mojo::Binding<shell::mojom::Service> catalog_binding(&resolver, std::move(resolver_request));

  shell::ServiceManager * service_manager = new shell::ServiceManager(std::move(runner_factory), std::move(resolver_ptr));

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

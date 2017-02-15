#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include <string>
#include "services/shell/native_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include <memory>
#include "services/shell/service_manager.h"
/*

*/

/*
  void ChildProcessHostImpl::CreateChannelMojo() {
  // TODO(rockot): Remove |channel_id_| once this is the only code path by which
  // the Channel is created. For now it serves to at least mutually exclude
  // different CreateChannel* calls.
  DCHECK(channel_id_.empty());
  channel_id_ = "ChannelMojo";

  shell::InterfaceProvider* remote_interfaces = GetRemoteInterfaces();
  DCHECK(remote_interfaces);

  IPC::mojom::ChannelBootstrapPtr bootstrap;
  remote_interfaces->GetInterface(&bootstrap);
  channel_ = IPC::ChannelMojo::Create(bootstrap.PassInterface().PassHandle(), IPC::Channel::MODE_SERVER, this);
  DCHECK(channel_);

  bool initialized = InitChannel();
  DCHECK(initialized);
}

  shell::InterfaceProvider* remote_interfaces = GetRemoteInterfaces();
  IPC::mojom::ChannelBootstrapPtr bootstrap;
  remote_interfaces->GetInterface(&bootstrap);

  mojo::ScopedMessagePipeHandle handle = mojo::edk::CreateChildMessagePipe(service_request_token);
  service_manager_connection_ = ServiceManagerConnection::Create(mojo::MakeRequest<shell::mojom::Service>(std::move(handle)), GetIOTaskRunner());
  service_manager_connection_->SetupInterfaceRequestProxies(GetInterfaceRegistry(), remote_interfaces);
  
  
  
    void StartWithClientProcessConnection(
      mojom::ClientProcessConnectionPtr client_process_connection) {
    mojom::ServicePtr service;
    service.Bind(mojom::ServicePtrInfo(
        std::move(client_process_connection->service), 0));
    pid_receiver_binding_.Bind(
        std::move(client_process_connection->pid_receiver_request));
    StartWithService(std::move(service));
  }
 

*/
#include "samples/mojo/service_manager_hello/some_interface.mojom.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/cpp/service_runner.h"

#include "samples/mojo/service_manager_hello/some_interface.mojom.h"
#include "services/shell/public/cpp/service.h"

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
#include "services/shell/public/cpp/interface_registry.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"

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

//  void OnStart(const shell::Identity& identity) override {
//    printf("DummyCatalog::OnStart identity name = %s user_id = %s instance = %s\n", identity.name().c_str(), identity.user_id().c_str(), identity.instance().c_str());
//  }

//  bool OnConnect(const shell::Identity& remote_identity, shell::InterfaceRegistry* registry) override {
//    printf("DummyCatalog::OnConnect remote_identity name = %s user_id = %s instance = %s registry = %p\n", remote_identity.name().c_str(), remote_identity.user_id().c_str(), remote_identity.instance().c_str(), registry);
    
//    registry->AddInterface<mojom::Catalog>(this);
//    registry->AddInterface<filesystem::mojom::Directory>(this);
//    registry->AddInterface<shell::mojom::Resolver>(this);

//    return true;
//  }

//  bool OnStop() override {
//    printf("DummyCatalog::OnStop\n");
//    return true;
//  }
  
};


#include "services/shell/native_runner.h"

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
//  std::unique_ptr<catalog::Store> store;
//  catalog::Catalog * catalog = new catalog::Catalog(blocking_pool_.get(), std::move(store), nullptr);
  shell::mojom::ServicePtr catalog_ptr;
  shell::mojom::ServiceRequest catalog_request = mojo::GetProxy(&catalog_ptr);
  
  DummyCatalog catalog;
  mojo::Binding<shell::mojom::Service> catalog_binding(&catalog, std::move(catalog_request));

  
  shell::ServiceManager * service_manager = new shell::ServiceManager(std::move(runner_factory), std::move(catalog_ptr));
  shell::mojom::ServiceRequest my_service_request = service_manager->StartEmbedderService("service:my_service");

  shell::Service * my_service = new MyService();
  std::unique_ptr<shell::ServiceContext> context(new shell::ServiceContext(my_service, mojo::MakeRequest<shell::mojom::Service>(my_service_request.PassMessagePipe())));
  my_service->set_context(std::move(context)); // from shell::ServiceRunner(new MyService).Run(service_request_handle)
  
//  mojo::Binding<shell::mojom::Service> my_service_binding(&my_service, std::move(my_service_request));

/*
  shell::Identity target("exe:my_service", shell::mojom::kRootUserID);
  std::unique_ptr<shell::ConnectParams> params(new shell::ConnectParams());
  params->set_target(target);
  params->set_source(target);

  service_manager->Connect(std::move(params));

*/

/*
// from ServiceManagerContext::ServiceManagerContext  

  std::unique_ptr<BuiltinManifestProvider> manifest_provider = base::MakeUnique<BuiltinManifestProvider>();

  static const struct ManifestInfo {
    const char* name;
    int resource_id;
  } kManifests[] = {
    { kBrowserServiceName, IDR_MOJO_CONTENT_BROWSER_MANIFEST },
    { kGpuServiceName, IDR_MOJO_CONTENT_GPU_MANIFEST },
    { kPluginServiceName, IDR_MOJO_CONTENT_PLUGIN_MANIFEST },
    { kRendererServiceName, IDR_MOJO_CONTENT_RENDERER_MANIFEST },
    { kUtilityServiceName, IDR_MOJO_CONTENT_UTILITY_MANIFEST },
    { "service:catalog", IDR_MOJO_CATALOG_MANIFEST },
    { file::kFileServiceName, IDR_MOJO_FILE_MANIFEST }
  };

  for (size_t i = 0; i < arraysize(kManifests); ++i) {
    std::string contents = GetContentClient()->GetDataResource(kManifests[i].resource_id, ui::ScaleFactor::SCALE_FACTOR_NONE).as_string();
    std::unique_ptr<base::Value> manifest_value = base::JSONReader::Read(contents);
    std::unique_ptr<base::Value> overlay_value = GetContentClient()->browser()->GetServiceManifestOverlay(kManifests[i].name);
    if (overlay_value) {
      base::DictionaryValue* manifest_dictionary = nullptr;
      CHECK(manifest_value->GetAsDictionary(&manifest_dictionary));
      base::DictionaryValue* overlay_dictionary = nullptr;
      CHECK(overlay_value->GetAsDictionary(&overlay_dictionary));
      MergeDictionary(manifest_dictionary, overlay_dictionary);
    }
    manifest_provider->AddManifestValue(kManifests[i].name, std::move(manifest_value));
  }

  shell::mojom::ServicePtr embedder_service_proxy;
  shell::mojom::ServiceRequest embedder_service_request = mojo::GetProxy(&embedder_service_proxy);
  shell::mojom::ServicePtrInfo embedder_service_proxy_info = embedder_service_proxy.PassInterface();

  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  std::unique_ptr<shell::NativeRunnerFactory> native_runner_factory(new shell::InProcessNativeRunnerFactory(blocking_pool));
  catalog_.reset(new catalog::Catalog(blocking_pool, nullptr, manifest_provider_.get()));
  service_manager_.reset(new shell::ServiceManager(std::move(native_runner_factory), catalog_->TakeService()));

  shell::mojom::ServiceRequest request = service_manager_->StartEmbedderService(kBrowserServiceName);
  mojo::FuseInterface(std::move(request), std::move(embedder_service_proxy_info));

  ServiceManagerConnection::SetForProcess(ServiceManagerConnection::Create(std::move(request), BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)));
  
  ContentBrowserClient::StaticServiceMap services;
  GetContentClient()->browser()->RegisterInProcessServices(&services);
  for (const auto& entry : services) {
    ServiceManagerConnection::GetForProcess()->AddEmbeddedService(entry.first, entry.second);
  }
  ServiceManagerConnection::GetForProcess()->Start();

  ContentBrowserClient::OutOfProcessServiceMap unsandboxed_services;
  GetContentClient()->browser()->RegisterUnsandboxedOutOfProcessServices(&unsandboxed_services);
  for (const auto& service : unsandboxed_services) {
    ServiceManagerConnection::GetForProcess()->AddServiceRequestHandler(service.first, base::Bind(&StartServiceInUtilityProcess, service.first, service.second, false ));
  }
  ServiceManagerConnection::GetForProcess()->AddServiceRequestHandler("service:media", base::Bind(&StartServiceInGpuProcess, "service:media"));
*/

/*
  std::unique_ptr<shell::Connection> connection,
  shell::mojom::ServiceRequest request)

  shell::mojom::ServicePtr service_ptr;
  shell::mojom::ServiceRequest serice_request = mojo::GetProxy(&service_ptr);
  ServiceManagerConnection * service_manager_connection = ServiceManagerConnection::Create(serice_request);
  mojom::SomeInterfacePtr some_interface;
  service_manager_connection->GetInterface(&some_interface);
  some_interface->Foo();
   
  std::unique_ptr<service_manager::Connection> connection = connector()->Connect("service:other_service");


  
//  base::Process process;
//  test::LaunchAndConnectToProcess("lifecycle_unittest_exe.exe", Identity(kTestExeName, mojom::kInheritUserID), connector(), &process);
  
  shell::mojom::ServiceFactoryPtr factory;
  factory.Bind(mojo::InterfacePtrInfo<service_manager::mojom::ServiceFactory>(std::move(pipe), 0u));
  shell::mojom::PIDReceiverPtr receiver;

  shell::Identity target("exe:target",service_manager::mojom::kInheritUserID);
  shell::Connector::ConnectParams params(target);
  params.set_client_process_connection(std::move(factory), MakeRequest(&receiver));
  std::unique_ptr<shell::Connection> connection = connector->Connect(&params);

  base::LaunchOptions options;
  options.handles_to_inherit = &info;
  base::Process process = base::LaunchProcess(target_command_line, options);
  mojo::edk::ChildProcessLaunched(process.Handle(), pair.PassServerHandle());
*/
  base::RunLoop().Run();
}


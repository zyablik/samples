#include "base/at_exit.h"
#include "base/json/json_reader.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include <memory>
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include <string>

#include "content/browser/loader/resource_handler.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/common/process_type.h"
#include "content/public/browser/resource_context.h"

#include "net/base/net_export.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_cert_types.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
extern "C" {
#include <coresrv/syscalls.h>
}
#include <kos/thread.h>
#include <sys/time.h>

int n = 0;
void * stress_fs(void *) {
    while(true) {
        int fd = open("/romfs/browser", O_RDWR);
        printf("[tid = %d] stress_fs fd = %d\n", KnThreadCurrent(), fd);
        while(true) {
//         for(int i = 0; i < 10000000; i++) {
//             n += 1;
//             n *= n;
//         }
        sleep(1);
//        printf("hellp %d\n", n);
        }

        if(fd < 0) {
            sleep(1);
            continue;
        }
        off_t length = lseek(fd, 0, SEEK_END);
        printf("[tid = %d] stress_fs started fd = %d file size = %d Kb RAND_MAX = %d\n", KnThreadCurrent(), fd, length / 1024, RAND_MAX);
        char buf[256 * 1024];
        while(true) {
            off_t next_seek = rand() % (length - sizeof(buf));
            off_t cur_seek = lseek(fd, next_seek, SEEK_SET);
            int ret = read(fd, buf, sizeof(buf));
//            printf("cur_seek = %d ret = %d\n", cur_seek, ret);
        }
    }
    return 0;
}

void download_request(const char * url, int id);

uint16_t crc16_mcrf4xx(const uint8_t * data, size_t len) {
    uint16_t crc = 0;
    if (!data || len < 0)
        return crc;

    while (len--) {
        crc ^= *data++;
        for (int i=0; i<8; i++) {
            if (crc & 1)  crc = (crc >> 1) ^ 0x8408;
            else          crc = (crc >> 1);
        }
    }
    return crc;
}

class DummyResourceHandler: public content::ResourceHandler {
 public:
  DummyResourceHandler(net::URLRequest * request, int id): content::ResourceHandler(request), id(id)
  {
    gettimeofday(&start, NULL);
    printf("[tid = %d] [this = %p] DummyResourceHandler::DummyResourceHandler(): start url = %s id=*%d*\n", KnThreadCurrent(), this, request->url().spec().c_str(), id);
    buffer_size_ = 256 * 1024;
    buffer_ = new net::IOBuffer(buffer_size_);
  };

  bool OnRequestRedirected(const net::RedirectInfo& redirect_info, content::ResourceResponse* response, bool* defer) override {
    printf("%s\n", __PRETTY_FUNCTION__);
    return true;
  };

  bool OnResponseStarted(content::ResourceResponse* response, bool* defer) override {
//     printf("%s\n", __PRETTY_FUNCTION__);
    return true;
  };

  bool OnWillStart(const GURL& url, bool* defer) override {
//     printf("%s\n", __PRETTY_FUNCTION__);
    return true;
  };

  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf, int* buf_size, int min_size) override {
//  printf("%s min_size = %d\n", __PRETTY_FUNCTION__, min_size);

    *buf = buffer_;
    *buf_size = buffer_size_;

    return true;
  };

  bool OnReadCompleted(int bytes_read, bool* defer) override {
//    printf("[this = %p] DummyResourceHandler::OnReadCompleted bytes_read = %d\n", this, bytes_read);

    body_.append(buffer_->data(), bytes_read);

    return true;
  };

  void OnResponseCompleted(const net::URLRequestStatus& status, bool* defer) override {
    static std::map<int, size_t> counters;
    counters[id]++;
    std::string status_str;
    switch(status.status()) {
        case net::URLRequestStatus::SUCCESS:    status_str = "SUCCESS"; break;
        case net::URLRequestStatus::IO_PENDING: status_str = "IO_PENDING"; break;
        case net::URLRequestStatus::CANCELED:   status_str = "CANCELED"; break;
        case net::URLRequestStatus::FAILED:     status_str = "FAILED"; break;
        default:                                status_str = "unknown"; break;
    }

    struct timeval finish;
    gettimeofday(&finish, NULL);
    uint64_t delta = finish.tv_sec - start.tv_sec;

    uint16_t crc = crc16_mcrf4xx((const uint8_t *)body_.data(), body_.size());
    printf("[tid = %d][this = %p] DummyResourceHandler::OnResponseCompleted(): finished url = %s crc = \033[0;33m0x%4x\033[0m id = *%d* counter = %d %s status = %d: %s \033[0m error() = %d body size = %d Kb load time = %d secs\n",
           KnThreadCurrent(), this, request()->url().spec().c_str(), crc, id, counters[id], status.status() == net::URLRequestStatus::SUCCESS ? "\033[0;32m" : "\033[0;31m", status.status(), status_str.c_str(), status.error(), body_.size() / 1024, delta);
//    printf("body = %s\n", body_.c_str());
    download_request(request()->url().spec().c_str(), id);
  };

  void OnDataDownloaded(int bytes_downloaded) override {
    printf("%s\n", __PRETTY_FUNCTION__);
  };

 private:
  struct timeval start;
  scoped_refptr<net::IOBuffer> buffer_;
  size_t buffer_size_;
  std::string body_;
  int id;
};

class DummyResourceLoaderDelegate: public content::ResourceLoaderDelegate {
public:
  content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(content::ResourceLoader * loader, net::AuthChallengeInfo * auth_info) {
      printf("%s\n", __PRETTY_FUNCTION__);
      return nullptr;
  }

  bool HandleExternalProtocol(content::ResourceLoader * loader, const GURL& url) override {
//      printf("%s\n", __PRETTY_FUNCTION__);
      return false;
  };

  void DidStartRequest(content::ResourceLoader* loader) override {
//      printf("%s\n", __PRETTY_FUNCTION__);
  };

  void DidReceiveRedirect(content::ResourceLoader* loader, const GURL& new_url, content::ResourceResponse* response) override {
      printf("%s\n", __PRETTY_FUNCTION__);
  };

  void DidReceiveResponse(content::ResourceLoader* loader) override {
//      printf("%s\n", __PRETTY_FUNCTION__);
  };

  void DidFinishLoading(content::ResourceLoader* loader) override {
//      printf("%s\n", __PRETTY_FUNCTION__);
  };

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(content::ResourceLoader* loader) override {
      printf("%s\n", __PRETTY_FUNCTION__);
      return nullptr;
  };
};

class DummyResourceContext: public content::ResourceContext {
 public:

  explicit DummyResourceContext(net::URLRequestContext * url_request_context, net::HostResolver * host_resolver): url_request_context_(url_request_context), host_resolver_(host_resolver) {};

  net::URLRequestContext* GetRequestContext() override {
    printf("%s\n", __PRETTY_FUNCTION__);
    return url_request_context_;
  };

  net::HostResolver * GetHostResolver() override {
    printf("%s\n", __PRETTY_FUNCTION__);
    return host_resolver_;
  };

 private:
  net::URLRequestContext * url_request_context_;
  net::HostResolver * host_resolver_;
};

class MockCTVerifier : public net::CTVerifier {
 public:
  MockCTVerifier() {}
  ~MockCTVerifier() override {}

  int Verify(net::X509Certificate* cert,
             const std::string& stapled_ocsp_response,
             const std::string& sct_list_from_tls_extension,
             net::SignedCertificateTimestampAndStatusList* output_scts,
             const net::NetLogWithSource& net_log) override
  {
    printf("MockCTVerifier::Verify stapled_ocsp_response = %s sct_list_from_tls_extension = %s\n", stapled_ocsp_response.c_str(), sct_list_from_tls_extension.c_str());
    return net::OK;
  }

  void SetObserver(net::CTVerifier::Observer* observer) override {}
};

class MockCertVerifier: public net::CertVerifier {
 public:
  int Verify(const RequestParams& params,
             net::CRLSet * crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<net::CertVerifier::Request> * out_req,
             const net::NetLogWithSource& net_log) override
  {
    printf("MockCertVerifier::Verify\n");
//    verify_result->verified_cert = cert;
    verify_result->is_issued_by_known_root = true;
    return 0;
  };
};

class MockCTPolicyEnforcer: public net::CTPolicyEnforcer {
 public:
  net::ct::CertPolicyCompliance DoesConformToCertPolicy(
      net::X509Certificate * cert,
      const net::SCTList& verified_scts,
      const net::NetLogWithSource& net_log) override
  {
    printf("MockCTPolicyEnforcer::DoesConformToCertPolicy\n");
    return net::ct::CertPolicyCompliance::CERT_POLICY_COMPLIES_VIA_SCTS;
  }
};

class MockExpectCTReporter: public net::TransportSecurityState::ExpectCTReporter {
 public:

  void OnExpectCTFailed(const net::HostPortPair& host_port_pair,
                        const GURL& report_uri,
                        const net::SSLInfo& ssl_info) override {
    printf("%s\n", __PRETTY_FUNCTION__);
  }
};

net::URLRequest * create_request(const char * url_str) {
  GURL url(url_str);

  net::URLRequestContext * url_request_context = new net::URLRequestContext();

// url_request_context_getter_qt.cpp ports\qt5.10\qtwebengine\src\core

  /*static*/ net::URLRequestJobFactoryImpl * job_factory = new net::URLRequestJobFactoryImpl();
  url_request_context->set_job_factory(job_factory);

//  url_request_context.set_cert_verifier(new MockCertVerifier());
  /*static*/ net::CertVerifier * cert_verifier = net::CertVerifier::CreateDefault().release();
  url_request_context->set_cert_verifier(cert_verifier);

  /*static*/ net::MultiLogCTVerifier * ct_verifier = nullptr;
  if(!ct_verifier) {
    ct_verifier = new net::MultiLogCTVerifier();
    ct_verifier->AddLogs(net::ct::CreateLogVerifiersForKnownLogs());
  }
//   url_request_context.set_cert_transparency_verifier(new MockCTVerifier());
  url_request_context->set_cert_transparency_verifier(ct_verifier);

  /*static*/ net::SSLConfigService * ssl_config_service = new net::SSLConfigServiceDefaults();
  url_request_context->set_ssl_config_service(ssl_config_service);

//  MockExpectCTReporter reporter;
//  net::TransportSecurityState transport_security_state;
//  transport_security_state.enable_static_expect_ct_ = true;
//  transport_security_state.SetExpectCTReporter(&reporter);
//  url_request_context.set_transport_security_state(&transport_security_state);

  /*static*/ net::TransportSecurityState * transport_security_state = new net::TransportSecurityState();
  url_request_context->set_transport_security_state(transport_security_state);

  /*static*/ net::HostResolver * host_resolver = net::HostResolver::CreateDefaultResolver(NULL).release();
  url_request_context->set_host_resolver(host_resolver);

  /*static*/ net::HttpAuthHandlerFactory * http_auth_handler_factory = net::HttpAuthHandlerFactory::CreateDefault(host_resolver).release();
  url_request_context->set_http_auth_handler_factory(http_auth_handler_factory);

  /*static*/ net::HttpServerProperties * http_server_properties = new net::HttpServerPropertiesImpl();
  url_request_context->set_http_server_properties(http_server_properties);

//  url_request_context.set_ct_policy_enforcer(new MockCTPolicyEnforcer());
  url_request_context->set_ct_policy_enforcer(new net::CTPolicyEnforcer());

  /*static*/ net::ProxyService * proxy_service = net::ProxyService::CreateFixed(net::ProxyConfig()).release();
  url_request_context->set_proxy_service(proxy_service); // no proxy

  net::HttpNetworkSession::Params network_session_params;

  network_session_params.transport_security_state     = url_request_context->transport_security_state();
  network_session_params.cert_verifier                = url_request_context->cert_verifier();
  network_session_params.channel_id_service           = url_request_context->channel_id_service();
  network_session_params.proxy_service                = url_request_context->proxy_service();
  network_session_params.ssl_config_service           = url_request_context->ssl_config_service();
  network_session_params.http_auth_handler_factory    = url_request_context->http_auth_handler_factory();
  network_session_params.http_server_properties       = url_request_context->http_server_properties();
  network_session_params.ignore_certificate_errors    = true;
  network_session_params.host_resolver                = url_request_context->host_resolver();
  network_session_params.cert_transparency_verifier   = url_request_context->cert_transparency_verifier();
  network_session_params.ct_policy_enforcer           = url_request_context->ct_policy_enforcer();

  /*static*/ net::HttpNetworkSession * http_network_session = new net::HttpNetworkSession(network_session_params);

  // own cache for every request to force them to be really downloaded
  net::HttpCache * cache = new net::HttpCache(http_network_session, net::HttpCache::DefaultBackend::InMemory(0), false);
  url_request_context->set_http_transaction_factory(cache);

  net::RequestPriority priority = net::MEDIUM;
  net::URLRequest * request = url_request_context->CreateRequest(url, priority, nullptr).release();

  content::ResourceContext * resource_context = new DummyResourceContext(url_request_context, host_resolver);

  content::ResourceRequestInfoImpl * info = new content::ResourceRequestInfoImpl(
    content::PROCESS_TYPE_RENDERER,                 // process_type
    0,                                     // child_id
    0,                                     // route_id
    -1,                                    // frame_tree_node_id
    0,                                     // origin_pid
    0,                                     // request_id
    0,                                     // render_frame_id
    true,                                 // is_main_frame
    false,                                 // parent_is_main_frame
    content::RESOURCE_TYPE_MAIN_FRAME,                   // resource_type
    ui::PAGE_TRANSITION_LINK,              // transition_type
    false,                                 // should_replace_current_entry
    false,                                 // is_download
    false,                                 // is_stream
    false,                                 // allow_download
    false,                                 // has_user_gesture
    false,                                 // enable load timing
    false,                                 // enable upload progress
    false,                                 // do_not_prompt_for_login
    blink::WebReferrerPolicyDefault,       // referrer_policy
    blink::WebPageVisibilityStateVisible,  // visibility_state
    resource_context,               // context
    nullptr,                 // filter
    false,                                 // report_raw_headers
    true,                                  // is_async
    false,                                 // is_using_lofi
    std::string(),                         // original_headers
    nullptr,                               // body
    false);                                // initiated_in_secure_context

  info->AssociateWithRequest(request);

  return request;
}

void download_request(const char * url, int id) {
    std::unique_ptr<net::URLRequest> request(create_request(url));
//     std::unique_ptr<net::URLRequest> request(create_request());

//  ResourceDispatcherHostImpl resource_dispatcher_host_impl;
//  std::unique_ptr<content::ResourceHandler> resource_handler(new AsyncResourceHandler(request.get(), &));
//  resource_handler.reset(new MojoAsyncResourceHandler(request, this, std::move(mojo_request), std::move(url_loader_client)));

    std::unique_ptr<content::ResourceHandler> resource_handler(new DummyResourceHandler(request.get(), id)); // RedirectToFileResourceHandler
    content::ResourceLoaderDelegate * resource_loader_delegate = new DummyResourceLoaderDelegate();
    content::ResourceLoader * loader = new content::ResourceLoader(std::move(request), std::move(resource_handler), resource_loader_delegate);
    loader->StartRequest();
}

void * dl_thread(void * id) {
  printf("[tid = %d] dl_thread id = %d\n", KnThreadCurrent(), id);
  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  download_request("https://docs.collabio.team/editor/js/textfilter.10.0.10.2006.js", (intptr_t) id);
//download_request("http://owwlz.com/textfilter.10.0.10.2006.js", (intptr_t) id);
//  download_request("http://10.100.3.145:8080", (intptr_t) id);

  base::RunLoop().Run();
  return nullptr;
}

__attribute__((visibility("default")))
int resourse_loader_main(int argc, char* argv[]) {
  printf("%s: main()\n", argv[0]);

//   pthread_t thread;
//   pthread_create(&thread, NULL, stress_fs, NULL);
//   sleep(1);

  base::CommandLine::Init(argc, argv);

  base::AtExitManager exit_manager;

  mojo::edk::Init();

  pthread_t dl;
  pthread_create(&dl, NULL, dl_thread, (void *)1);
  pthread_create(&dl, NULL, dl_thread, (void *)2);
  pthread_create(&dl, NULL, dl_thread, (void *)3);

  base::MessageLoop loop(base::MessageLoop::TYPE_IO);

  
// DummyProcessDelegate delegate;
// mojo::edk::InitIPCSupport(&delegate, loop.task_runner());

//  base::i18n::InitializeICU();

//     download_request("https://docs.collabio.team/editor/js/textfilter.10.0.10.2006.js", 1);
//    download_request("https://docs.collabio.team/editor/js/textfilter.10.0.10.2006.js", 2);
//    download_request("https://docs.collabio.team/editor/js/textfilter.10.0.10.2006.js", 3);
//   download_request("http://az.lib.ru/t/tolstoj_lew_nikolaewich/text_0080.shtml", 1);
//   download_request("http://owwlz.com/textfilter.10.0.10.2006.js", 1);
//  download_request("http://10.100.3.145:8080/", 1);
//   download_request("http://10.100.3.145:8080/", 2);
//   download_request("http://10.100.3.145:8080/", 3);

  base::RunLoop().Run();

  return 0;
}

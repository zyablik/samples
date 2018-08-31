#include <fcntl.h>

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_message_utils.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/process_delegate.h"

#include "my_channel_listener.h"

using namespace std;

const char kMojoNamedPipeName[] = "mojo-named-pipe-name";
const char * program;
std::unique_ptr<IPC::Channel> client_channel;

void hello_from_delayed_task() {
    printf("[%s] hello from client task. Exit in two seconds.\n", program);
}

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};


void do_in_io_thread() {
    printf("[%s] do_in_io_thread()\n", program);
    mojo::edk::NamedPlatformHandle named_pipe(base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(kMojoNamedPipeName));
    mojo::ScopedMessagePipeHandle master_pipe = mojo::edk::ConnectToPeerProcess(mojo::edk::CreateClientHandle(named_pipe));
    printf("[%s] mojo::edk::ConnectToPeerProcess('%s') master_pipe->value() = %d\n", program, named_pipe.name.c_str(), master_pipe->value());

    MyChannelListener * client_channel_listener = new MyChannelListener();
    client_channel = IPC::ChannelMojo::Create(std::move(master_pipe), IPC::Channel::MODE_CLIENT, client_channel_listener, base::MessageLoop::current()->task_runner());
    printf("[%s] client_channel = %p\n", program, client_channel.get());
    bool result = client_channel->Connect();
    printf("[%s] client_channel.Connect() = %d\n", program, result);

    SendIntStringPairMessage(client_channel.get(), "hello from client");
    SendIntStringPairMessage(client_channel.get(), "hello from client");

    base::FileDescriptor descriptor;
    const int fd = open(program, O_RDONLY);
    descriptor.auto_close = true;
    descriptor.fd = fd;
    printf("send file descriptor %d\n",fd);

    IPC::Message* message = new IPC::Message(0, MessageType::FileDescriptor, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
    client_channel->Send(message);
}

int main(int argc, char** argv) {
    base::AtExitManager exit_manager;
    program = argv[0];
    printf("[%s] CommandLine = '", program);
    for(int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("'\n");

    base::CommandLine::Init(argc, argv);
    mojo::edk::Init();

    base::Thread io_thread("io thread");
    CHECK(io_thread.StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
    DummyProcessDelegate delegate;
    mojo::edk::InitIPCSupport(&delegate, io_thread.message_loop()->task_runner());

    io_thread.message_loop()->task_runner()->PostTask(FROM_HERE, base::Bind(&do_in_io_thread));
    io_thread.message_loop()->task_runner()->PostDelayedTask(FROM_HERE, base::Bind(&hello_from_delayed_task), base::TimeDelta::FromSeconds(1));
    io_thread.message_loop()->task_runner()->PostDelayedTask(FROM_HERE, io_thread.message_loop()->QuitWhenIdleClosure(), base::TimeDelta::FromSeconds(3));

    while(io_thread.IsRunning()) sleep(1);
    printf("io_thread exit\n");

    client_channel->Close();
    mojo::edk::ShutdownIPCSupport();

    return 0;
}

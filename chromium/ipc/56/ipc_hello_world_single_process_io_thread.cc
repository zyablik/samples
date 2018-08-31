#include <sys/syscall.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/edk/embedder/process_delegate.h"

#include "my_channel_listener.h"

using namespace std;

void hello_from_delayed_task() {
    cout << "Hello from delayed task. Exit in two seconds.\n";
}

const char * program;
std::unique_ptr<IPC::Channel> client_channel;
std::unique_ptr<IPC::Channel> server_channel;

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

void do_in_io_thread() {
    printf("[tid = %d] do_in_io_thread\n", syscall(SYS_gettid));
    mojo::MessagePipe pipe;

    MyChannelListener * server_channel_listener = new MyChannelListener();
    server_channel = IPC::ChannelMojo::Create(std::move(pipe.handle0), IPC::Channel::MODE_SERVER, server_channel_listener, base::MessageLoop::current()->task_runner());
    printf("server_channel = %p\n", server_channel.get());

    bool result = server_channel->Connect();
    printf("server_channel.Connect() = %d\n", result);

    MyChannelListener * client_channel_listener = new MyChannelListener();
    client_channel = IPC::ChannelMojo::Create(std::move(pipe.handle1), IPC::Channel::MODE_CLIENT, client_channel_listener, base::MessageLoop::current()->task_runner());
    printf("client_channel = %p\n", client_channel.get());
    result = client_channel->Connect();
    printf("client_channel.Connect() = %d\n", result);

    SendIntStringPairMessage(server_channel.get(), "hello from server");
    SendIntStringPairMessage(server_channel.get(), "hello from server");

    SendIntStringPairMessage(client_channel.get(), "hello from client");
    SendIntStringPairMessage(client_channel.get(), "hello from client");

    base::FileDescriptor descriptor;
    const int fd = open(program, O_RDONLY);
    descriptor.auto_close = true;
    descriptor.fd = fd;
    cout << "send file descriptor " << dec << fd << endl;

    IPC::Message* message = new IPC::Message(0, MessageType::FileDescriptor, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
    client_channel->Send(message);
}

int main(int argc, char** argv) {
    program = argv[0];
    base::CommandLine::Init(argc, argv);
    base::AtExitManager exit_manager;
    mojo::edk::Init();

    base::Thread io_thread("io thread");
    CHECK(io_thread.StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));

    DummyProcessDelegate delegate;
    mojo::edk::InitIPCSupport(&delegate, io_thread.message_loop()->task_runner());

    io_thread.message_loop()->task_runner()->PostTask(FROM_HERE, base::Bind(&do_in_io_thread));
    io_thread.message_loop()->task_runner()->PostDelayedTask(FROM_HERE, base::Bind(&hello_from_delayed_task), base::TimeDelta::FromSeconds(1));
    io_thread.message_loop()->task_runner()->PostDelayedTask(FROM_HERE, io_thread.message_loop()->QuitWhenIdleClosure(), base::TimeDelta::FromSeconds(3));

    while(io_thread.IsRunning()) sleep(1);
    sleep(1);
    printf("io_thread exit\n");
//    server_channel->Close();
//    client_channel->Close();
    mojo::edk::ShutdownIPCSupport();

    return 0;
}

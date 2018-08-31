#include <fcntl.h>
#include <unistd.h>

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

// inspired by mojo/edk/embedder/embedder_unittest.cc

using namespace std;

const char kMojoNamedPipeName[] = "mojo-named-pipe-name";
const char * program;
std::unique_ptr<IPC::Channel> server_channel;

class DummyProcessDelegate: public mojo::edk::ProcessDelegate {
public:
  void OnShutdownComplete() override {
    printf("[%s] DummyProcessDelegate::OnShutdownComplete\n", program);
  }
};

void hello_from_delayed_task() {
    printf("[%s] hello from server delayed task\n", program);
}

void do_in_io_thread() {
    printf("[tid = %lu] do_in_io_thread\n", pthread_self());
    base::FilePath temp_dir;
    CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
    mojo::edk::NamedPlatformHandle named_pipe = mojo::edk::NamedPlatformHandle(temp_dir.AppendASCII(mojo::edk::GenerateRandomToken()).value());
    base::CommandLine command_line(base::CommandLine::StringVector {"./ipc-hello-multi-process-client"});
    command_line.AppendSwitchNative(kMojoNamedPipeName, named_pipe.name);

    mojo::ScopedMessagePipeHandle slave_pipe = mojo::edk::ConnectToPeerProcess(mojo::edk::CreateServerHandle(named_pipe));
    printf("[%s] ConnectToPeerProcess('%s') slave_pipe->value() = %d\n", program, named_pipe.name.c_str(), slave_pipe->value());

    base::Process slave = base::LaunchProcess(command_line, base::LaunchOptions());

    MyChannelListener * server_channel_listener = new MyChannelListener();
    server_channel = IPC::ChannelMojo::Create(std::move(slave_pipe), IPC::Channel::MODE_SERVER, server_channel_listener, base::MessageLoop::current()->task_runner());
    printf("[%s] server_channel = %p\n", program, server_channel.get());
    bool result = server_channel->Connect();
    printf("[%s] server_channel.Connect() = %d\n", program, result);

    SendIntStringPairMessage(server_channel.get(), "hello from server");
    SendIntStringPairMessage(server_channel.get(), "hello from server");

    base::FileDescriptor descriptor;
    const int fd = open(program, O_RDONLY);
    descriptor.auto_close = true;
    descriptor.fd = fd;
    printf("send file descriptor %d\n", fd);

    IPC::Message* message = new IPC::Message(0, MessageType::FileDescriptor, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
    server_channel->Send(message);
}

int main(int argc, char** argv) {
    program = argv[0];
    printf("[tid = %lu] main()\n", pthread_self());

    base::AtExitManager exit_manager;

    mojo::edk::Init();

    base::Thread io_thread("io thread");
    CHECK(io_thread.StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));

    DummyProcessDelegate delegate;
    mojo::edk::InitIPCSupport(&delegate, io_thread.message_loop()->task_runner());

    io_thread.message_loop()->task_runner()->PostTask(FROM_HERE, base::Bind(&do_in_io_thread));
    io_thread.message_loop()->task_runner()->PostDelayedTask(FROM_HERE, base::Bind(&hello_from_delayed_task), base::TimeDelta::FromSeconds(2));
    io_thread.message_loop()->task_runner()->PostDelayedTask(FROM_HERE, io_thread.message_loop()->QuitWhenIdleClosure(), base::TimeDelta::FromSeconds(3));

    while(io_thread.IsRunning()) sleep(1);
    sleep(1);
    printf("io_thread exit\n");
//    server_channel->Close();
    mojo::edk::ShutdownIPCSupport();

    return 0;
}

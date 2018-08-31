#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"

#include "my_channel_listener.h"

using namespace std;

void hello_from_delayed_task() {
    cout << "Hello from delayed task. Exit in two seconds.\n";
}

const char * program;

int main(int argc, char** argv) {
    program = argv[0];
    base::CommandLine::Init(argc, argv);
    base::AtExitManager exit_manager;
    base::MessageLoop message_loop;
    base::RunLoop run_loop;
    mojo::edk::Init();

    mojo::MessagePipe pipe;

    MyChannelListener server_channel_listener;
    std::unique_ptr<IPC::Channel> server_channel = IPC::ChannelMojo::Create(std::move(pipe.handle0), IPC::Channel::MODE_SERVER, &server_channel_listener, base::ThreadTaskRunnerHandle::Get());
    printf("server_channel = %p\n", server_channel.get());

    bool result = server_channel->Connect();
    printf("server_channel.Connect() = %d\n", result);

    MyChannelListener client_channel_listener;
    std::unique_ptr<IPC::Channel> client_channel = IPC::ChannelMojo::Create(std::move(pipe.handle1), IPC::Channel::MODE_CLIENT, &client_channel_listener, base::ThreadTaskRunnerHandle::Get());
    printf("client_channel = %p\n", client_channel.get());
    result = client_channel->Connect();
    printf("client_channel.Connect() = %d\n", result);

    SendIntStringPairMessage(server_channel.get(), "hello from server");
    SendIntStringPairMessage(server_channel.get(), "hello from server");

    SendIntStringPairMessage(client_channel.get(), "hello from client");
    SendIntStringPairMessage(client_channel.get(), "hello from client");

    base::FileDescriptor descriptor;
    const int fd = open(argv[0], O_RDONLY);
    descriptor.auto_close = true;
    descriptor.fd = fd;
    cout << "send file descriptor " << dec << fd << endl;

    IPC::Message* message = new IPC::Message(0, MessageType::FileDescriptor, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
    client_channel->Send(message);

    base::MessageLoop::current()->task_runner()->PostDelayedTask(FROM_HERE, base::Bind(&hello_from_delayed_task), base::TimeDelta::FromSeconds(1));

    base::MessageLoop::current()->task_runner()->PostDelayedTask(FROM_HERE, base::MessageLoop::current()->QuitWhenIdleClosure(), base::TimeDelta::FromSeconds(3));
    base::RunLoop().Run();
//    base::RunLoop().RunUntilIdle();
    server_channel->Close();
    client_channel->Close();
    return 0;
}

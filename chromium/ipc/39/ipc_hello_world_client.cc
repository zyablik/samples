#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_posix.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/unix_domain_socket_util.h"

#include "my_channel_listener.h"
#include "setup_client.h"
#include "setup_server.h"

//#include "ui/surface/transport_dib.h"

using namespace std;

void hello_from_delayed_task() {
    cout << "hello from client task. Exit in two seconds.\n";
}

int main(int argc, char** argv) {
    base::AtExitManager at_exit_manager_;

    base::MessageLoopForIO * main_message_loop = new base::MessageLoopForIO();

    int client_socket_fd = setup_client_socket(kChannelName);
    if(client_socket_fd < 0) {
        cout << "error while setup client socket. exit\n";
        return -1;
    }

    IPC::ChannelHandle client_handle(kChannelName);
    client_handle.socket.fd = client_socket_fd;

    MyChannelListener client_channel_listener;

    IPC::ChannelPosix client_channel(client_handle, IPC::Channel::MODE_NAMED_CLIENT, &client_channel_listener);
    cout << "client_channel_listener = " << hex << &client_channel_listener << endl;
    if(!client_channel.Connect()){
        cout << "client_channel connect\n"; return -1;
    }

    SendIntStringPairMessage(&client_channel, "hello from client");
    SendIntStringPairMessage(&client_channel, "hello from client");

    base::FileDescriptor descriptor;
    const int fd = open(argv[0], O_RDONLY);
    descriptor.auto_close = true;
    descriptor.fd = fd;
    cout << "send file descriptor " << dec << fd << endl;

    IPC::Message* message = new IPC::Message(0, MessageType::FileDescriptor, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
    client_channel.Send(message);

    base::MessageLoopForIO::current()->PostDelayedTask(FROM_HERE, base::Bind(&hello_from_delayed_task), base::TimeDelta::FromSeconds(3));
    base::MessageLoopForIO::current()->Run();

    cout << "message loop exit\n";
    delete main_message_loop;

    return 0;
}

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

const char * kChannelName = "/tmp/IPCChannelPosixTest_BasicListen";

enum MessageType {
    IntStringPair,
    FileDescriptor,
    TransportDIBDescriptor
};

std::vector<std::string> MessageTypeStrings {"IntStringPair", "FileDescriptor", "TransportDIBDescriptor"};

using namespace std;

void SendIntStringPairMessage(IPC::Sender * sender, const char * text) {
  static int message_index = 0;

  IPC::Message* message = new IPC::Message(0, MessageType::IntStringPair, IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(++message_index);
  message->WriteString(std::string(text));

  // DEBUG: printf("[%u] sending message [%s]\n", GetCurrentProcessId(), text);
  cout << "Send MessageType::IntStringPair id = " << message_index << " text = " << text << endl; 
  sender->Send(message);
}

class MyChannelListener: public IPC::Listener {
public:
    bool OnMessageReceived(const IPC::Message& message) override {
        cout << "MyChannelListener::OnMessageReceived type = " << message.type();
        base::PickleIterator iter(message);
        switch(message.type()) {
            case MessageType::IntStringPair: {
                int msgid;
                bool result = iter.ReadInt(&msgid);
                if(!result) {
                    cout << "\nMyChannelListener::OnMessageReceived: error while ReadInt\n";
                    break;
                }

                std::string payload;
                result = iter.ReadString(&payload);
                if(!result) {
                    cout << "\nMyChannelListener::OnMessageReceived error while ReadString\n";
                    break;
                }
                cout << " ('" << MessageTypeStrings[message.type()] << ")' id = " << msgid << " " << payload <<"\n";
                break;
            }
            case MessageType::FileDescriptor: {
                base::FileDescriptor descriptor;
                IPC::ParamTraits<base::FileDescriptor>::Read(&message, &iter, &descriptor);
                cout << " ('" << MessageTypeStrings[message.type()] << ")'  fd = " << dec << descriptor.fd << "\n";
                break;
            }
            case MessageType::TransportDIBDescriptor: {
                base::FileDescriptor descriptor;
                IPC::ParamTraits<base::FileDescriptor>::Read(&message, &iter, &descriptor);
                cout << " ('" << MessageTypeStrings[message.type()] << ")'  fd = " << dec << descriptor.fd << " ";
//                TransportDIB * dib = TransportDIB::Map(descriptor);
//                cout << " message: " << (char*)dib->memory() << endl;
                break;
            }
            default:
                cout << ": unknown message type\n";
        }
        return true;
    }

    void OnChannelConnected(int32_t peer_pid) override {
        cout << "MyChannelListener::OnChannelConnected " << hex << this << " peer_pid = " << peer_pid << endl;
    }

    void OnChannelError() override {
        cout << "MyChannelListener::OnChannelError " << hex << this << endl;
    }

    void OnChannelDenied() override {
        cout << "MyChannelListener::OnChannelDenied " << hex << this << endl;
    }

    void OnChannelListenError() override {
        cout << "OnChannelListenError " << hex << this << endl;
    }
};
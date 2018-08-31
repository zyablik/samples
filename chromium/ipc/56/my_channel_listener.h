#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_message.h"
#include <iostream>
#include "ipc/ipc_message_utils.h"

enum MessageType {
    IntStringPair,
    FileDescriptor,
};

std::vector<std::string> MessageTypeStrings {"IntStringPair", "FileDescriptor"};

using namespace std;

extern const char * program;

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
    MyChannelListener() {
        printf("[%s] MyChannelListener::MyChannelListener()\n", program);
    }

    ~MyChannelListener() {
        printf("[%s] MyChannelListener::~MyChannelListener()\n", program);
    }
    
    bool OnMessageReceived(const IPC::Message& message) override {
        printf("[%s] MyChannelListener::OnMessageReceived type = %d", program, message.type());
        base::PickleIterator iter(message);
        switch(message.type()) {
            case MessageType::IntStringPair: {
                int msgid;
                bool result = iter.ReadInt(&msgid);
                if(!result) {
                    printf("\nMyChannelListener::OnMessageReceived: error while ReadInt\n");
                    break;
                }

                std::string payload;
                result = iter.ReadString(&payload);
                if(!result) {
                    printf("\nMyChannelListener::OnMessageReceived error while ReadString\n");
                    break;
                }
                printf(" ('%s') id = %d %s\n", MessageTypeStrings[message.type()].c_str(), msgid, payload.c_str());
                break;
            }
            case MessageType::FileDescriptor: {
                base::FileDescriptor descriptor;
                IPC::ParamTraits<base::FileDescriptor>::Read(&message, &iter, &descriptor);
                printf(" ('%s')  fd = %d\n", MessageTypeStrings[message.type()].c_str(), descriptor.fd);
                break;
            }
            default:
                printf(": unknown message type\n");
        }
        return true;
    }

    void OnChannelConnected(int32_t peer_pid) override {
        printf("[%s] MyChannelListener::OnChannelConnected this = %p peer_pid = %d\n", program, this, peer_pid);
    }

    void OnChannelError() override {
        printf("[%s] MyChannelListener::OnChannelError this = %p\n", program, this);
    }

    void OnChannelDenied() override {
        printf("[%s] MyChannelListener::OnChannelDenied this = %p\n", program, this);
    }

    void OnChannelListenError() override {
        printf("[%s] MyChannelListener::OnChannelListenError this = %p\n", program, this);
    }
};

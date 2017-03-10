#include <seppo/common/SharedMemory.hpp>
#include <seppo/common/Thread.hpp>
#include <seppo/protocol/ParcelMessage.hpp>
#include <seppo/protocol/Server.hpp>
#include <vector>
#include <algorithm>    // std::find
#include <unistd.h>

class SharedMemoryImpl: public seppo::SharedMemory {
public:
    SharedMemoryImpl(std::string const& name, int fd, int size): seppo::SharedMemory(name, fd, size) {}

    
protected:
    void write(char * data, unsigned int size) override {
        printf("SharedMemoryImpl write size = %d data = %s\n", size, data);
    }
};

class CommandServer: public seppo::Server
{
public:
    CommandServer(): seppo::Server("seppo") {
        printf("[pid = %d] CommandServer::CommandServer('seppo'): waiting for incoming connections\n", getpid());
        connectShellOpened(sigc::mem_fun(this, &CommandServer::handleShellOpened));
        connectShellClosed(sigc::mem_fun(this, &CommandServer::handleClosed));
        connectParcelMessage(sigc::mem_fun(this, &CommandServer::handleParcelMessage));
    }

private:
    void handleShellOpened(unsigned int clientId) {
        printf("CommandServer::handleShellOpened() client id = %d\n", clientId);
        clients_.push_back(clientId);
    }

    void handleClosed(int id) {
        printf("CommandServer::handleClosed client id = %d\n", id);
        auto it = std::find(clients_.begin(), clients_.end(), id);
        if(it != clients_.end()) {
            printf("CommandServer::handleClosed remove client %d\n", id);
            clients_.erase(it);
        }
    }

    // Managing commands
    void handleParcelMessage(int id, const seppo::ReferencedPointer<seppo::ParcelMessage> &msg) {
        printf("CommandServer::handleParcelMessage client_id = %d ParcelMessage[type = %d messageId = %d senderId = %d messageSize = %d messageDataByteSize = %d ancillaryDataLen = %d]\n",
               id, msg->type(), msg->messageId(), msg->senderId(), msg->messageSize(), msg->messageDataByteSize(), msg->ancillaryDataLen());

        /* find client */
        auto it = std::find(clients_.begin(), clients_.end(), id);
        if(it != clients_.end()) {
            if(msg->ancillaryDataLen() == 0) {
                int size = msg->messageDataByteSize();
                printf("CommandServer::handleParcelMessage send echo response '%s'\n", msg->messageDataPointer());
                seppo::ReferencedPointer<seppo::ParcelMessage> reply_msg = seppo::ParcelMessage::createResultTemplate(msg, size);
                memcpy(reply_msg->messageDataPointer(), msg->messageDataPointer(), size);
                sendParcelMessage(id, reply_msg);
            } else {
                seppo::Parcel * parcel = seppo::ParcelMessage::getParcel(msg);
                seppo::AbstractMessage::MsgHeader header;
                parcel->read((char*)&header, sizeof(seppo::AbstractMessage::MsgHeader));
                int shm_fd, shm_size;
                parcel->readFd(shm_fd);
                parcel->read(shm_size);

                printf("CommandServer::handleParcelMessage with shm fd received: shm_fd = %d shm_size = %d\n", shm_fd, shm_size);
                
                SharedMemoryImpl shm("seppo_server_test_shm", shm_fd, shm_size);
                char shm_reply_msg[] = "shm hello from server";
                shm.seppo::SharedMemory::write(shm_reply_msg, sizeof(shm_reply_msg), 1000);
                shm.update(0);

                seppo::ReferencedPointer<seppo::ParcelMessage>  reply_msg = seppo::ParcelMessage::createCommandTemplate();
                parcel = seppo::ParcelMessage::getParcel(reply_msg);
                const char msg_reply_msg[] = "msg hello from server";
                parcel->write(msg_reply_msg, sizeof(msg_reply_msg));
                sendParcelMessage(id, reply_msg);
            }
        } else {
            printf("doResponse for client %i, client id doesn't exist\n", id);
        }
    }

    std::vector<int> clients_;
};

int main(int, char *[]) {
    CommandServer server;
    server.runLoop();
    return 0;
}

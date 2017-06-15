#include <seppo/common/SharedMemory.hpp>
#include <seppo/common/Thread.hpp>
#include <seppo/protocol/Client.hpp>
#include <seppo/protocol/Parcel.hpp>
#include <seppo/protocol/ParcelMessage.hpp>
#include <sstream>
#include <string>
#include <unistd.h>

class CommandClient: public seppo::Client, public seppo::Thread
{
public:
     CommandClient(): seppo::Client("seppo") // open connection to Server (named with "seppo")
    {
        connectShellOpened(sigc::mem_fun(this,&CommandClient::handleOpen));
        connectShellClosed(sigc::mem_fun(this,&CommandClient::handleClosed));
        connectParcelMessage(sigc::mem_fun(this,&CommandClient::handleParcelMessage));
    }

    void run() override {
        printf("[tid = %d] CommandClient::run\n", gettid());
        runLoop();
    }

    void handleOpen() {
        printf("CommandClient::handleOpen\n");
    }

    void handleClosed() {
        printf("CommandClient::handleClosed\n");
    }

    void handleParcelMessage(const seppo::ReferencedPointer<seppo::ParcelMessage>& command) {
        const char * msg = command->messageDataPointer();
        printf("CommandClient::handleParcelMessage ParcelMessage[type = %d messageId = %d senderId = %d messageSize = %d messageDataByteSize = %d] msg = '%s'\n",
               command->type(), command->messageId(), command->senderId(), command->messageSize(), command->messageDataByteSize(), msg);
    }
};

class SharedMemoryImpl: public seppo::SharedMemory {
public:
    SharedMemoryImpl(std::string const& name, int size): seppo::SharedMemory(name, size) {}

    void write(char * data, unsigned int size) override {
        printf("SharedMemoryImpl write size = %d data = %s\n", size, data);
    }
};

int main(int, char *[])
{
    printf("press enter to create seppo::Client\n");
    char buf[32];
    gets(buf);

    CommandClient client;
    
//    printf("press enter to client.start()\n");
//    gets(buf);

//    looks like this is not necessary    
//    client.start();

    printf("press enter to send msg\n");
    gets(buf);
    
    char msg[] = "hello from client";
    seppo::ReferencedPointer<seppo::ParcelMessage> simple_command = seppo::ParcelMessage::createCommandTemplate(sizeof(msg) + 1);
    strcpy(simple_command->messageDataPointer(), msg);
    client.sendParcelMessage(simple_command);

    auto shm_command = seppo::ParcelMessage::createCommandTemplate();
    auto parcel = seppo::ParcelMessage::getParcel(shm_command);

    printf("press enter to send msg\n");
    gets(buf);

    size_t shm_size = 1024;
    SharedMemoryImpl shm("seppo_client_test_shm", shm_size);
    shm.dataReceived.connect([&](char * data, size_t size) {
        printf("shm->dataReceived size = %zu, data = '%s'\n", size, data);
        sleep(1);
        shm.stopListening();
    });

    parcel->writeFd(shm.fd());
    parcel->write((int)shm_size);
    
    client.sendParcelMessage(shm_command, [](const seppo::ReferencedPointer<seppo::ParcelMessage>& msg) {
        printf("parcel message callback msg->id = %d msg->messageId() = %d reply = %s\n", msg->id(), msg->messageId(), msg->messageDataPointer());
    });

    shm.listen();
    return 0;
}

#include "adler32.hpp"
#include <seppo/protocol/Application.hpp>
#include <seppo/protocol/ParcelMessage.hpp>
#include <seppo/common/ReferencedPointer.hpp>
#include <seppo/common/String.hpp>
#include <set>
#include <vanda/Compositor.hpp>
#include <vanda/MessageFactory.hpp>
#include <vanda/TextureConnection.hpp>
#include <vanda/MeshConnection.hpp>
#include <utils.hpp>

class SimpleTexture : public vanda::TextureConnection {
public:
    CLASS_COPYABLE(SimpleTexture)

    SimpleTexture(unsigned int id, std::string const& name, int fd, int size): vanda::TextureConnection(name, fd, size), id(id) {
        printf("SimpleTexture::SimpleTexture id = %d name = %s fd = %d size = %d\n", id, name.c_str(), fd, size);
    }

    unsigned id;
};

class SimpleMesh : public vanda::MeshConnection
{
public:
    CLASS_COPYABLE(SimpleMesh)

    SimpleMesh(unsigned int id, std::string const& name, int fd, int size): vanda::MeshConnection(name, fd, size), id(id) {
        printf("SimpleMesh::SimpleMesh id = %d name = %s fd = %d size = %d\n", id, name.c_str(), fd, size);
    }

    unsigned id;
};

class HashReader: public vanda::AbstractSharedMemoryResourceConsumer
{
public:
    HashReader(int id, int size, vanda::MessageFactory::MessageType::Enum type): id_(id), size_(size), type_(type) {
        printf("HashReader::HashReader id = %d size = %d type_ = %d\n", id_, size_, type_);
    }

    void read(char * data) override {
        int header_size = 0;
        if(type_ == vanda::MessageFactory::MessageType::Client_UpdateTexture) {
            header_size = 3 * sizeof(uint32_t);
        } else if(type_ == vanda::MessageFactory::MessageType::Client_UpdateMesh) {
            header_size = sizeof(uint32_t);
        }

        printf("HashReader::read id = %d size = %d header_size = %d type_ = %d data = %p\n", id_, size_, header_size, type_, data);

        if(size_ > header_size) {
            int * header = (int *) data;
            printf(" %dx%d format = %d hash = %d", header[1], header[2], header[0], adler32((unsigned char*) data + header_size, (size_ - header_size)));
        }
        printf("\n");
    }

private:
    int id_;
    int size_;
    vanda::MessageFactory::MessageType::Enum type_;
};

class SimpleCompositor : public seppo::Application, public seppo::Thread {
public:
    SimpleCompositor(): textureUpdated_(false), meshUpdated_(false) {}

    ~SimpleCompositor() {
        textureConnections_.clear();
        meshConnections_.clear();
    }

protected:

    void run() {
        printf("[tid = %d] SimpleCompositor::run \n", gettid());
        compositor.connectClientShellOpened(sigc::mem_fun(this, &SimpleCompositor::handleConnectClientShellOpened));
        compositor.connectClientShellClosed(sigc::mem_fun(this, &SimpleCompositor::handleConnectClientShellClosed));
        compositor.connectParcelMessage(sigc::mem_fun(this, &SimpleCompositor::handleParcelMessage));
        compositor.connectParcelArray(sigc::mem_fun(this, &SimpleCompositor::handleParcelMessages));
        compositor.start();

        while(true) {
            sleep(1);
            checkHash();
            seppo::Lock lock(mutex_);

//             for(uint32_t client : clients) {
//                 seppo::ReferencedPointer<seppo::ParcelMessage> resultMessage = vanda::MessageFactory::serverUpdateCommand();
//                 printf("Server_Update for %d\n", client);
//                 compositor.sendParcelMessage(client, resultMessage);
//             }

            compositor.update();
        }

        printf("[tid = %d] SimpleCompositor::run(): exit() \n", gettid());
    }

    void processEvent(const seppo::ReferencedPointer<seppo::ParcelMessage>& message) override {
        unsigned int clientId = message->senderId();
        printf("[tid = %d] SimpleCompositor::processEvent msg received: '%s'\n", gettid(), seppoParcelMessage2str(*message.pointer()).c_str());
        
        if(message->type() == seppo::MessageType::Parcel_Command) {
            seppo::Parcel * parcel = seppo::ParcelMessage::getParcel(message);
            unsigned int * data = (unsigned int *)message->messageDataPointer();
            vanda::MessageFactory::MessageType::Enum type = (vanda::MessageFactory::MessageType::Enum)data[0];
            switch(type) {
                case vanda::MessageFactory::MessageType::Client_VolumeOpen: {
                    int screenResolutionX = 1080;
                    int screenResolutionY = 1920;
                    // Respond result
                    seppo::ReferencedPointer<seppo::ParcelMessage> resultMessage = 
                        vanda::MessageFactory::clientVolumeOpenCommandResult(message, true, screenResolutionX, screenResolutionY, {}, 0);
                    compositor.sendParcelMessage(clientId, resultMessage);
                    break;
                }

                case vanda::MessageFactory::MessageType::Client_UpdateTexture: {
                    if(data[2] == vanda::AbstractTextureConnection::Type::SharedMemory) {
                        std::string name = "vanda" + seppo::String::toString(clientId) + "texture" + seppo::String::toString(data[1]);
                        int fd;
                        parcel->readFd(fd);
                        printf("SimpleCompositor::processEvent Client_UpdateTexture SharedMemory fd = %d name = %s\n", fd, name.c_str());
                        processTextureUpdate(clientId, name, data[1], fd, data[6], data[4], data[5]);
                        seppo::ReferencedPointer<seppo::ParcelMessage> resultMessage = vanda::MessageFactory::clientCommandResult(message);
                        compositor.sendParcelMessage(clientId, resultMessage);
                    }
                    break;
                }

                case vanda::MessageFactory::MessageType::Client_UpdateMesh: {
                    std::string name = "vanda" + seppo::String::toString(clientId) + "mesh" + seppo::String::toString(data[1]);
                    if(data[2] == 0){
                        int fd;
                        parcel->readFd(fd);
                        printf("SimpleCompositor::processEvent Client_UpdateMesh fd = %d", fd);
                        processMeshUpdate(clientId, name, data[1], fd, data[3]);
                        seppo::ReferencedPointer<seppo::ParcelMessage> resultMessage = vanda::MessageFactory::clientCommandResult(message);
                        compositor.sendParcelMessage(clientId, resultMessage);
                    } else {
                        printf(" error: data[2] = %d", data[2]);
                    }
                    break;
                }

                case vanda::MessageFactory::MessageType::Client_VolumeControl: {
                    vanda::MessageFactory::VolumeControl::Enum type = (vanda::MessageFactory::VolumeControl::Enum)data[1];
                    if(type == vanda::MessageFactory::VolumeControl::Resume) {
                        compositor.resume(clientId);
                    } else {
                        printf("SimpleCompositor::processEvent: skip\n");
                    }
                    break;
                }

                case vanda::MessageFactory::MessageType::Client_VolumeClose: {
                    seppo::Lock lock(mutex_);
                    compositor.removeShell(clientId);
                    removeClientSpace(clientId);
                    break;
                }
                default:
                    printf("SimpleCompositor::processEvent: skip\n");
            }
        } else {
            printf(" :unknown message->type() %d, do nothing\n", message->type());
        }
    }

    void processTextureUpdate(unsigned int clientId, std::string const& name, unsigned int objectId, int fd, int size, int width, int height) {
        printf("SimpleCompositor::processTextureUpdate clientId = %d name = %s objectId = %d fd = %d size = %d width = %d height = %d\n", clientId, name.c_str(), objectId, fd, size, width, height);
        seppo::Lock lock(mutex_);
        std::map<const std::string, seppo::ReferencedPointer<SimpleTexture> >::iterator i = textureConnections_.find(name);

        if(i == textureConnections_.end()) {
            // create new connection
            seppo::ReferencedPointer<SimpleTexture> newConnection(new SimpleTexture(clientId, name, fd, size));
            textureConnections_.insert(std::make_pair(name, newConnection));
        } else {
            // update existing connection
            SimpleTexture * existingConnection = i->second.pointer();

            if(existingConnection->name() == name) {
                close(fd);
                // using the same shared memory block
                existingConnection->setDirty();
            } else {
                seppo::ReferencedPointer<SimpleTexture> newBlock(new SimpleTexture(clientId, name, fd, size));
                i->second = newBlock;
            }
        }

        textureUpdated_ = true;
    }

    void processMeshUpdate(unsigned int clientId, std::string const& name, unsigned int objectId, int fd, int size) {
        printf("SimpleCompositor::processMeshUpdate clientId = %d name = %s objectId = %d fd = %d size = %d\n", clientId, name.c_str(), objectId, fd, size);
        seppo::Lock lock(mutex_);
        std::map<const std::string, seppo::ReferencedPointer<SimpleMesh> >::iterator i = meshConnections_.find(name);

        if(i == meshConnections_.end()) {
            // create new connection
            seppo::ReferencedPointer<SimpleMesh> newConnection(new SimpleMesh(clientId, name, fd, size));
            meshConnections_.insert(std::make_pair(name, newConnection));
        } else {
            // update existing connection
            SimpleMesh * existingConnection = i->second.pointer();

            if(existingConnection->name() == name) {
                // using the same shared memory block
                existingConnection->setDirty();
            } else {
                seppo::ReferencedPointer<SimpleMesh> newBlock(new SimpleMesh(clientId, name, fd, size));
                i->second = newBlock;
            }
        }

        meshUpdated_ = true;
    }

    void removeClientSpace(unsigned clientId) {
        printf("SimpleCompositor::removeClientSpace clientId = %d\n", clientId);
        std::map<const std::string, seppo::ReferencedPointer<SimpleTexture> >::iterator prev = textureConnections_.end();
        std::map<const std::string, seppo::ReferencedPointer<SimpleTexture> >::iterator it;

        for (it = textureConnections_.begin(); it != textureConnections_.end(); ++it) {
            if(prev != textureConnections_.end()) {
                textureConnections_.erase(prev);
                prev = textureConnections_.end();
            }

            if(clientId == it->second->id)
                prev = it;
        }

        if(prev != textureConnections_.end())
            textureConnections_.erase(prev);
    }

private:

    void handleConnectClientShellOpened(unsigned int clientId) {
        printf("[tid = %d] SimpleCompositor::handleConnectClientShellOpened clientId = %d\n", gettid(), clientId);
    }

    void handleConnectClientShellClosed(unsigned int clientId) {
        printf("[tid = %d] SimpleCompositor::handleConnectClientShellClosed clientId = %d\n", gettid(), clientId);
    }

    void handleParcelMessage(unsigned int clientId, const seppo::ReferencedPointer<seppo::ParcelMessage>& message) {
        printf("[tid = %d] SimpleCompositor::handleParcelMessage clientId = %d: addEvent(message, false)\n", gettid(), clientId);
        addEvent(message, false);
    }

    void handleParcelMessages(unsigned int clientId, const std::vector<seppo::ReferencedPointer<seppo::ParcelMessage> >& messages) {
        printf("[tid = %d] SimpleCompositor::handleParcelMessages clientId = %d: addEvent(messages, false) \n", gettid(), clientId);
        addEvent(messages, false);
    }

    void checkHash() {
        printf("checkHash\n");
        seppo::Lock lock(mutex_);
        if(textureUpdated_) {
            textureUpdated_ = false;
            std::map<const std::string, seppo::ReferencedPointer<SimpleTexture> >::iterator it;
            for(it = textureConnections_.begin(); it != textureConnections_.end(); ++it) {
                auto abstractConnection = it->second;
                if(abstractConnection->isDirty()) {
                    HashReader reader(it->second->id, it->second->size(), vanda::MessageFactory::MessageType::Client_UpdateTexture);
                    it->second->read(&reader);
                }
            }
        }

        if(meshUpdated_) {
            meshUpdated_ = false;
            std::map<const std::string, seppo::ReferencedPointer<SimpleMesh> >::iterator it;
            for (it = meshConnections_.begin(); it != meshConnections_.end(); ++it) {
                auto abstractConnection = it->second;
                if(abstractConnection->isDirty()) {
                    HashReader reader(it->second->id, it->second->size(), vanda::MessageFactory::MessageType::Client_UpdateMesh);
                    it->second->read(&reader);
                }
            }
        }
    }

    bool textureUpdated_;
    bool meshUpdated_;
    vanda::Compositor compositor;
    std::map<const std::string, seppo::ReferencedPointer<SimpleTexture> > textureConnections_;
    std::map<const std::string, seppo::ReferencedPointer<SimpleMesh> > meshConnections_;
    seppo::Mutex mutex_;
    std::set<uint32_t> clients;
};

int main(int, char **) {
    SimpleCompositor compositor;
    compositor.start();
    compositor.runLoop();
    return 0;
}

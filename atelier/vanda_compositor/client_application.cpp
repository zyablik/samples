#include "adler32.hpp"
#include <vanda/Client.hpp>
#include <vanda/ClientApplication.hpp>
#include <utils.hpp>

void gen_random(char *s, const int len) {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int pid = getpid();
    sprintf(s, "%d", pid);
    std::string svalue(s);
    int d = svalue.length();
    s[d] = ':';
    for (int i = (d+1); i < len; ++i)
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];

    s[len] = 0;
}

class SimpleClient: public vanda::ClientApplication, public seppo::Thread {
public:

    SimpleClient(): vanda::ClientApplication(10, 10, 0), texture_(1024, 1024, vanda::Texture::Format::RGBA8888) {
        texture_.connectWriteHandler(sigc::mem_fun(this, &SimpleClient::textureUpdate));
        texture_.lock();
        update(texture_);
        volumeControl(vanda::MessageFactory::VolumeControl::Resume);
    }

    ~SimpleClient() {
        texture_.unlock();
    }

    void run(void) override {
        mainloop();
    }

    void processEvent(const seppo::ReferencedPointer<seppo::ParcelMessage>& msg) override {
        printf("[tid = %d] SimpleClient::processEvent: '%s'\n", gettid(), seppoParcelMessage2str(*msg.pointer()).c_str());
        vanda::ClientApplication::processEvent(msg);

//        clientApplication_.volumeControl(vanda::MessageFactory::VolumeControl::Show);
        
//        MessageFactory::clientVolumeControlCommand(vanda::MessageFactory::VolumeControl::Resume);
    }

    // Change the color in the texture, shm is locked during this call
    void textureUpdate(char * data) {
        unsigned int length = texture_.height() * texture_.width() * 4; // 4 comes from: texture format is 8888 RGBA
        gen_random(data,length-1); /* gen_random adds 0 in end of array + 1 */
        printf("[tid = %d] SimpleClient::textureUpdate data = %p new hash = %d\n", gettid(), data, adler32((unsigned char*) data, length));
    }

    void updateTexture() {
        printf("[tid = %d] SimpleClient::updateTexture getGroupId() = %d\n", gettid(), getGroupId());
        texture_.update(getGroupId());
        update(texture_);
    }

    private:
        vanda::Texture texture_;
};

int main(int, char **)
{
    SimpleClient client;
    client.start();
    while(true) {
        sleep(2);
        client.updateTexture();
    }

    return 0;
}

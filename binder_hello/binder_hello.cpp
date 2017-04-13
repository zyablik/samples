#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

android::String16 TestServiceName("TestSimpleServer");

class SimpleService : public android::BBinder {
public:
    SimpleService() {
        pipe(controlPipe);
        printf("SimpleService::SimpleService(): controlPipe[%d, %d]\n", controlPipe[0], controlPipe[1]);
    }

    enum commands {
        TEST_SERVICE_COMMAND = IBinder::FIRST_CALL_TRANSACTION,
        TEST_SERVICE_END     = (TEST_SERVICE_COMMAND + 1),
    };

    android::status_t onTransact(uint32_t code, const android::Parcel& data, android::Parcel * reply, uint32_t flags = 0) override {
        android::status_t rv(0);
        // handle wait outside of the main mutex to allow concurrent waits
        switch (code) {
            case TEST_SERVICE_COMMAND: {
                int32_t num = data.readInt32();
                printf("SimpleService::onTransact TEST_SERVICE_COMMAND received flags = 0x%x dataSize = %zu num = %d, write fd = %d\n", flags, data.dataSize(), num, controlPipe[0]);
                reply->writeInt32(num);
                reply->writeFileDescriptor(controlPipe[0]);
                reply->writeStrongBinder(this);
                break;
            }

            case TEST_SERVICE_END: {
                printf("SimpleService::onTransact TEST_SERVICE_END received flags = 0x%x\n", flags);
                break;
            }

            default:
                printf("SimpleService::onTransact unknown code = %d\n", code);
        }
        return rv;

    }

private:
    int controlPipe[2];
};

int main(int, char **) {
    android::ProcessState::self()->startThreadPool();
    android::sp<android::IServiceManager> service_manager = android::defaultServiceManager();
    int result = service_manager->addService(TestServiceName, new SimpleService());
    if (result != 0) {
        printf("addService %s failed, result: %i errno %d, %s\n", android::String8(TestServiceName).string(), result, errno, strerror(errno));
        exit(1);
    }

    printf("registered service %s\n", android::String8(TestServiceName).string());

    sleep(1); // wait for service start
    android::sp<android::IBinder> binder = service_manager->getService(TestServiceName);
    if (binder == NULL) {
        printf("binder service not started\n");
        exit(1);
    }

    android::Parcel msg;
    msg.writeInt32(666);
//    msg.setDataSize(4);

    android::Parcel reply;
    printf("send SimpleService::TEST_SERVICE_COMMAND num = 666\n");
    result = binder->transact(SimpleService::TEST_SERVICE_COMMAND, msg, &reply);
    if (result != 0) {
        printf("binder->transact(SimpleService::TEST_SERVICE_COMMAND) failed, result: %i errno %d, %s", result, errno, strerror(errno));
        exit(1);
    }

    int num = reply.readInt32();
    int fd = reply.readFileDescriptor();
    android::sp<android::IBinder> remote_binder = reply.readStrongBinder();

    printf("reply for SimpleService::TEST_SERVICE_COMMAND received: num = %d fd = %d remote_binder interface = %s\n", num, fd, android::String8(remote_binder->getInterfaceDescriptor()).string());

    android::Parcel msg2;
    printf("before SimpleService::TEST_SERVICE_END sent\n");
    binder->transact(SimpleService::TEST_SERVICE_END, msg2, NULL, android::IBinder::FLAG_ONEWAY);
    printf("after SimpleService::TEST_SERVICE_END sent\n");
    printf("join\n");
    android::IPCThreadState::self()->joinThreadPool(true);
    printf("exit");
    return 0;
}

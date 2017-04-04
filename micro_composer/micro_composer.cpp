#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include "binder_interfaces.h"
#include "hwc_native_window.h"
#include "native_window_stub.h"

class ComposerService : public android::BBinder {
public:
    ComposerService() {
        printf("ComposerService::ComposerService()\n");
    }

    android::status_t onTransact(uint32_t code, const android::Parcel& data, android::Parcel * reply, uint32_t flags = 0) override {
        switch (code) {
            case IMicroComposer::GET_WINDOW_TRANSACTION: {
                printf("ComposerService::onTransact GET_WINDOW_TRANSACTION received\n");
                reply->writeStrongBinder(new NativeWindowStub(static_cast<ANativeWindow *>(new HWCNativeWindow())));
                break;
            }

            default:
                printf("SimpleService::onTransact unknown code = %d\n", code);
        }
        return 0;
    }
};

int main(int, char **) {
    android::ProcessState::self()->startThreadPool();
    android::sp<android::IServiceManager> service_manager = android::defaultServiceManager();
    int result = service_manager->addService(android::String16(MICRO_COMPOSER_SERVICE_NAME), new ComposerService());
    if (result != 0) {
        printf("addService %s failed, result: %i errno %d, %s\n", MICRO_COMPOSER_SERVICE_NAME, result, errno, strerror(errno));
        exit(1);
    }
    android::IPCThreadState::self()->joinThreadPool(true);
    printf("exit");
    return 0;
}

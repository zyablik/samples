#include "binder_interfaces.h"
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include "native_window_buffer.h"
#include "native_window_proxy.h"
#include "utils.h"

NativeWindowProxy::NativeWindowProxy(): mRefCount(0) {
    ANativeWindow::common.incRef = NativeWindowProxy::_incRef;
    common.incRef(&common);
    ANativeWindow::common.decRef = NativeWindowProxy::_decRef;

    ANativeWindow::cancelBuffer = _cancelBuffer;
    ANativeWindow::query = _query;
    ANativeWindow::perform = _perform;
    ANativeWindow::setSwapInterval = _setSwapInterval;
    ANativeWindow::queueBuffer = _queueBuffer;
    ANativeWindow::dequeueBuffer = _dequeueBuffer;

    const_cast<uint32_t&>(ANativeWindow::flags) = 0;
    const_cast<float&>(ANativeWindow::xdpi) = 0;
    const_cast<float&>(ANativeWindow::ydpi) = 0;
    const_cast<int&>(ANativeWindow::minSwapInterval) = 0;
    const_cast<int&>(ANativeWindow::maxSwapInterval) = 0;

    android::ProcessState::self()->startThreadPool();

    android::sp<android::IServiceManager> service_manager = android::defaultServiceManager();
    android::sp<android::IBinder> composer_binder = service_manager->getService(android::String16(MICRO_COMPOSER_SERVICE_NAME));
    if (composer_binder == NULL) {
        printf("composer_binder is null\n");
        exit(1);
    }

    android::Parcel window_request, window_reply;
    android::status_t result = composer_binder->transact(IMicroComposer::GET_WINDOW_TRANSACTION, window_request, &window_reply);
    if (result != android::NO_ERROR) {
        printf("composer_binder->transact(IMicroComposer::GET_WINDOW_TRANSACTION) failed error = %s. exit(1).\n", android_status(result).c_str());
        exit(1);
    }

    native_window_binder = window_reply.readStrongBinder();
    if(native_window_binder == NULL) {
        printf("native_window_binder is null. exit(1).\n");
        exit(1);
    }
}

NativeWindowProxy::~NativeWindowProxy() {
    printf("NativeWindowProxy::~NativeWindowProxy this = %p\n", this);

    android::Parcel query, response;
    android::status_t result = native_window_binder->transact(INativeWindow::DESTROY_TRANSACTION, query, &response);
    if(result != android::NO_ERROR)
        printf("NativeWindowProxy::~NativeWindowProxy this = %p native_window_binder->transact(INativeWindow::DESTROY_TRANSACTION) failed result = %s\n", this, android_status(result).c_str());

    native_window_binder = NULL;
}

void NativeWindowProxy::_incRef(struct android_native_base_t * base) {
    ANativeWindow * anw = reinterpret_cast<ANativeWindow*>(base);
    NativeWindowProxy * self = static_cast<NativeWindowProxy *>(anw);
    __sync_fetch_and_add(&self->mRefCount, 1);
    printf("[tid = %d] NativeWindowProxy::_incRef self = %p mRefCount = %d\n", gettid(), self, self->mRefCount);
}

void NativeWindowProxy::_decRef(struct android_native_base_t * base) {
    ANativeWindow * anw = reinterpret_cast<ANativeWindow*>(base);
    NativeWindowProxy * self = static_cast<NativeWindowProxy *>(anw);
    printf("[tid = %d] NativeWindowProxy::_decRef self = %p mRerfCount = %d\n", gettid(), self, self->mRefCount - 1);
    if (__sync_fetch_and_sub(&self->mRefCount, 1) == 1) {
        delete self;
    }
}

int NativeWindowProxy::_cancelBuffer(ANativeWindow * window, ANativeWindowBuffer * buffer, int fenceFd) {
    printf("[tid = %d] NativeWindowProxy::_cancelBuffer window = %p buffer = %p fence = %d (not implemented)\n", gettid(), window, buffer, fenceFd);
    return 0;
}

int NativeWindowProxy::_query(const ANativeWindow * window, int what, int* value) {
    printf("[tid = %d] NativeWindowProxy::_query window = %p what = %s returned value = ", gettid(), window, native_query_operation(what).c_str());
    const NativeWindowProxy * self = static_cast<const NativeWindowProxy*>(window);
    android::Parcel query, response;
    query.writeInt32(what);
    android::status_t result = self->native_window_binder->transact(INativeWindow::QUERY_TRANSACTION, query, &response);
    if(result == android::NO_ERROR) {
        *value = response.readInt32();
        printf("%d\n", *value);
        return 0;
    } else {
        printf(" -1\nNativeWindowProxy::_query window = %p what = %s self->native_window_binder->transact() FAILED error = %s\n", window, native_query_operation(what).c_str(), android_status(result).c_str());
        exit(1);
//        *value = 0;
//        return -1;
    }
}

int NativeWindowProxy::_perform(ANativeWindow * window, int operation, ... ) {
    va_list args;
    va_start(args, operation);
    NativeWindowProxy * self = static_cast<NativeWindowProxy *>(window);
    printf("[tid = %d] NativeWindowProxy::_perform self = %p window = %p operation = %s", gettid(), self, window, native_window_operation(operation).c_str());
    android::Parcel query, response;
    query.writeInt32(operation);
    
    switch(operation) {
        case NATIVE_WINDOW_SET_USAGE:
        case NATIVE_WINDOW_SET_BUFFER_COUNT:
        case NATIVE_WINDOW_SET_BUFFERS_FORMAT:
        case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
            query.writeInt32(va_arg(args, int));
            break;

        case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
            query.writeInt32(va_arg(args, int));
            query.writeInt32(va_arg(args, int));
            break;

        default: break;
    }
    va_end(args);
    android::status_t result = self->native_window_binder->transact(INativeWindow::PERFORM_TRANSACTION, query, &response);
    if(result != android::NO_ERROR) {
        printf(" FAILED result = %s\n", android_status(result).c_str());
        exit(1);
    } else {
        printf("\n");
    }
    return 0;
}

int NativeWindowProxy::_setSwapInterval(ANativeWindow * window, int interval) {
    printf("NativeWindowProxy::_setSwapInterval window = %p interval = %d (not implemented)\n", window, interval);
    return 0;
}

int NativeWindowProxy::_dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer, int* fenceFd) {
    printf("NativeWindowProxy::_dequeueBuffer window = %p\n", window);
    NativeWindowProxy * self = static_cast<NativeWindowProxy *>(window);

    android::Parcel query, response;
    android::status_t result = self->native_window_binder->transact(INativeWindow::DEQUEUE_BUFFER_TRANSACTION, query, &response);
    if(result != android::NO_ERROR) {
        printf("native_window_binder->transact(DEQUEUE_BUFFER_TRANSACTION) FAILED! error = %s\n", android_status(result).c_str());
        exit(1);
    }

    NativeWindowBuffer * native_buffer = read_native_window_buffer_from_parcel(response);
    *buffer = static_cast<ANativeWindowBuffer *>(native_buffer);

    *fenceFd = dup(native_buffer->fenceFd);
    close(native_buffer->fenceFd);
    native_buffer->fenceFd = -1;

    return 0;
}

int NativeWindowProxy::_queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd) {
    printf("[tid = %d] NativeWindowProxy::_queueBuffer window = %p buffer = %p fence = %d\n", gettid(), window, buffer, fenceFd);
    NativeWindowProxy * self = static_cast<NativeWindowProxy *>(window);

    android::Parcel query, response;
    write_native_window_buffer_to_parcel(*static_cast<NativeWindowBuffer *>(buffer), query);
    android::status_t result = self->native_window_binder->transact(INativeWindow::QUEUE_BUFFER_TRANSACTION, query, &response);
    query.writeFileDescriptor(fenceFd);

    if(result != android::NO_ERROR) {
        printf("FAILED result = %s\n", android_status(result).c_str());
        exit(1);
    }

    buffer->common.decRef(&buffer->common);
    return 0;
}

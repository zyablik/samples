#include "binder_interfaces.h"
#include "native_window_stub.h"
#include "native_window_buffer.h"
#include "utils.h"

android::status_t NativeWindowStub::onTransact(uint32_t code, const android::Parcel& data, android::Parcel * reply, uint32_t flags)
{
    switch (code) {
        case INativeWindow::QUERY_TRANSACTION: {
            printf("[tid = %d] NativeWindowStub::onTransact QUERY_TRANSACTION received\n", gettid());
            int what = data.readInt32();
            int value;
            int result = native_window->query(native_window, what, &value);
            reply->writeInt32(value);
            break;
        }

        case INativeWindow::PERFORM_TRANSACTION: {
            printf("[tid = %d] NativeWindowStub::onTransact PERFORM_TRANSACTION received\n", gettid());
            int result;
            int operation = data.readInt32();
            switch(operation) {
                case NATIVE_WINDOW_SET_USAGE:
                case NATIVE_WINDOW_SET_BUFFER_COUNT:
                case NATIVE_WINDOW_SET_BUFFERS_FORMAT:
                case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
                    result = native_window->perform(native_window, operation, data.readInt32());
                    break;

                case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
                    result = native_window->perform(native_window, operation, data.readInt32(), data.readInt32());
                    break;

                default:
                    result = native_window->perform(native_window, operation);
            }
            reply->writeInt32(result);
            break;
        }

        case INativeWindow::DEQUEUE_BUFFER_TRANSACTION: {
            printf("[tid = %d] NativeWindowStub::onTransact DEQUEUE_BUFFER_TRANSACTION received\n", gettid());
            ANativeWindowBuffer * anative_buffer = nullptr;
            int fenceFd;
            native_window->dequeueBuffer(native_window, &anative_buffer, &fenceFd);
            write_native_window_buffer_to_parcel(*static_cast<NativeWindowBuffer *>(anative_buffer), *reply);
            static_cast<NativeWindowBuffer *>(anative_buffer)->ownGrMemory = false;
            anative_buffer->common.decRef(&anative_buffer->common);
            break;
        }

        case INativeWindow::QUEUE_BUFFER_TRANSACTION: {
            printf("[tid = %d] NativeWindowStub::onTransact QUEUE_BUFFER_TRANSACTION received\n", gettid());
            NativeWindowBuffer * native_buffer = read_native_window_buffer_from_parcel(data);
            int fenceFd = data.readFileDescriptor();
            native_window->queueBuffer(native_window, native_buffer, fenceFd);
            native_buffer->ownGrMemory = true;
            native_buffer->common.decRef(&native_buffer->common);
            break;
        }

        default:
            printf("NativeWindowStub::onTransact unknown code = %d\n", code);
    }
    return android::NO_ERROR;
}

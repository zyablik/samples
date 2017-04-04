#include "native_window_buffer.h"
#include "utils.h"
#include <sstream>
#include <system/window.h>

std::string android_status(android::status_t status) {
    switch(status) {
        case android::NO_ERROR: return "NO_ERROR";
        case android::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        case android::NO_MEMORY: return "NO_MEMORY";
        case android::INVALID_OPERATION: return "INVALID_OPERATION";
        case android::BAD_VALUE: return "BAD_VALUE";

        case android::BAD_TYPE: return "BAD_TYPE";
        case android::NAME_NOT_FOUND: return "NAME_NOT_FOUND";
        case android::PERMISSION_DENIED: return "PERMISSION_DENIED";
        case android::NO_INIT: return "NO_INIT";
        case android::ALREADY_EXISTS: return "ALREADY_EXISTS";
        case android::DEAD_OBJECT: return "DEAD_OBJECT";
        case android::FAILED_TRANSACTION: return "FAILED_TRANSACTION";
        case android::BAD_INDEX: return "BAD_INDEX";
        case android::NOT_ENOUGH_DATA: return "NOT_ENOUGH_DATA";
        case android::WOULD_BLOCK: return "WOULD_BLOCK";
        case android::TIMED_OUT: return "TIMED_OUT";
        case android::UNKNOWN_TRANSACTION: return "UNKNOWN_TRANSACTION";
        case android::FDS_NOT_ALLOWED: return "FDS_NOT_ALLOWED";
        default: 
            std::stringstream s;
            s << "UNKNOWN ANDROID::STATUS: " << status;
            return s.str();
    }
}    

std::string native_window_operation(int what) {
    switch (what) {
        case NATIVE_WINDOW_SET_USAGE: return "NATIVE_WINDOW_SET_USAGE";
        case NATIVE_WINDOW_CONNECT: return "NATIVE_WINDOW_CONNECT";
        case NATIVE_WINDOW_DISCONNECT: return "NATIVE_WINDOW_DISCONNECT";
        case NATIVE_WINDOW_SET_CROP: return "NATIVE_WINDOW_SET_CROP";
        case NATIVE_WINDOW_SET_BUFFER_COUNT: return "NATIVE_WINDOW_SET_BUFFER_COUNT";
        case NATIVE_WINDOW_SET_BUFFERS_GEOMETRY: return "NATIVE_WINDOW_SET_BUFFERS_GEOMETRY";
        case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM: return "NATIVE_WINDOW_SET_BUFFERS_TRANSFORM";
        case NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP: return "NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP";
        case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS: return "NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS";
        case NATIVE_WINDOW_SET_BUFFERS_FORMAT: return "NATIVE_WINDOW_SET_BUFFERS_FORMAT";
        case NATIVE_WINDOW_SET_SCALING_MODE: return "NATIVE_WINDOW_SET_SCALING_MODE";
        case NATIVE_WINDOW_LOCK: return "NATIVE_WINDOW_LOCK";
        case NATIVE_WINDOW_UNLOCK_AND_POST: return "NATIVE_WINDOW_UNLOCK_AND_POST";
        case NATIVE_WINDOW_API_CONNECT: return "NATIVE_WINDOW_API_CONNECT";
        case NATIVE_WINDOW_API_DISCONNECT: return "NATIVE_WINDOW_API_DISCONNECT";
        case NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS: return "NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS";
        case NATIVE_WINDOW_SET_POST_TRANSFORM_CROP: return "NATIVE_WINDOW_SET_POST_TRANSFORM_CROP";
        default: 
            std::stringstream s;
            s << "NATIVE_UNKNOWN_OPERATION: " << what;
            return s.str();
    }
}

std::string native_query_operation(int what) {
    switch (what) {
        case NATIVE_WINDOW_WIDTH: return "NATIVE_WINDOW_WIDTH";
        case NATIVE_WINDOW_HEIGHT: return "NATIVE_WINDOW_HEIGHT";
        case NATIVE_WINDOW_FORMAT: return "NATIVE_WINDOW_FORMAT";
        case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS: return "NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS";
        case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER: return "NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER";
        case NATIVE_WINDOW_CONCRETE_TYPE: return "NATIVE_WINDOW_CONCRETE_TYPE";
        case NATIVE_WINDOW_DEFAULT_WIDTH: return "NATIVE_WINDOW_DEFAULT_WIDTH";
        case NATIVE_WINDOW_DEFAULT_HEIGHT: return "NATIVE_WINDOW_DEFAULT_HEIGHT";
        case NATIVE_WINDOW_TRANSFORM_HINT: return "NATIVE_WINDOW_TRANSFORM_HINT";
        case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND: return "NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND";
        case NATIVE_WINDOW_DEFAULT_DATASPACE: return "NATIVE_WINDOW_DEFAULT_DATASPACE";
        default:
            std::stringstream s;
            s << "NATIVE_UNKNOWN_QUERY: " << what;
            return s.str();
    }
}

NativeWindowBuffer * read_native_window_buffer_from_parcel(const android::Parcel& parcel) {
    NativeWindowBuffer * buffer = new NativeWindowBuffer();
    
    buffer->width = parcel.readInt32();
    buffer->height = parcel.readInt32();
    buffer->stride = parcel.readInt32();
    buffer->format = parcel.readInt32();
    buffer->usage = parcel.readInt32();
    buffer->reserved[0] = 0;
    buffer->reserved[1] = 0;
    
    int numFds = parcel.readInt32();
    int numInts = parcel.readInt32();

    printf("read_native_window_buffer_from_parcel buffer[width = %d height = %d stride = %d format 0x%x usage = 0x%x] handle[numFds = %d numInts = %d]",
           buffer->width, buffer->height, buffer->stride, buffer->format, buffer->usage, numFds, numInts);
    // from frameworks/native/libs/binder/Parcel.cpp: Parcel::readNativeHandle()
    // native_handle * h = native_handle_create(numFds, numInts);

    // from system/core/libcutils.cpp: native_handle_create()
    native_handle_t * h = (native_handle_t *) malloc(sizeof(native_handle_t) + (sizeof(int) * (numFds + numInts)));
    h->version = sizeof(native_handle_t);
    h->numFds = numFds;
    h->numInts = numInts;

    printf(" fds = [");
    for(int i = 0; i < numFds; i++) {
        int orig = parcel.readFileDescriptor();
        h->data[i] = dup(orig);
        printf(" orig = %d safe = %d", orig, h->data[i]);
    }
    printf("]\n");
    parcel.read(h->data + numFds, sizeof(int) * numInts);
    buffer->handle = h;


    int fence_fd_presence_flag = parcel.readInt32();
    if(fence_fd_presence_flag) {
        buffer->fenceFd = parcel.readFileDescriptor();
    }
    return buffer;
}

void write_native_window_buffer_to_parcel(const NativeWindowBuffer& buffer, android::Parcel& parcel) {
    printf("write_native_window_buffer_to_parcel buffer[width = %d height = %d stride = %d format 0x%x usage = 0x%x] handle[numFds = %d numInts = %d]",
           buffer.width, buffer.height, buffer.stride, buffer.format, buffer.usage, buffer.handle->numFds, buffer.handle->numInts);
    parcel.writeInt32(buffer.width);
    parcel.writeInt32(buffer.height);
    parcel.writeInt32(buffer.stride);
    parcel.writeInt32(buffer.format);
    parcel.writeInt32(buffer.usage);
//    parcel.writeInt32(*buffer.reserved[0]);
//    parcel.writeInt32(*buffer.reserved[1]);

    // from frameworks/native/libs/binder/Parcel.cpp: Parcel::writeNativeHandle()
    // file-descriptors are dup'ed, so it is safe to delete the native_handle when this function returns
    parcel.writeInt32(buffer.handle->numFds);
    parcel.writeInt32(buffer.handle->numInts);
    printf(" fds = [");
    for(int i = 0; i < buffer.handle->numFds; i++) {
        int safe_fd = dup(buffer.handle->data[i]);
        printf("orig = %d safe = %d", buffer.handle->data[i], safe_fd);
        parcel.writeFileDescriptor(safe_fd, true /*takeOwnership*/);
    }
    printf("]\n");
    parcel.write(buffer.handle->data + buffer.handle->numFds, sizeof(int) * buffer.handle->numInts);

    if(buffer.fenceFd == -1) {
        parcel.writeInt32(0);
    } else {
        parcel.writeInt32(1);
        parcel.writeFileDescriptor(dup(buffer.fenceFd));
    }
}

#include <hardware/gralloc.h>
#include <stdlib.h>
#include "native_window_buffer.h"
#include <stdio.h>
#include <system/window.h>

hw_module_t * NativeWindowBuffer::gr_module = NULL;
alloc_device_t * NativeWindowBuffer::gr_dev = NULL;

NativeWindowBuffer::NativeWindowBuffer(): fenceFd(-1), mRefCount(0) {
    printf("[tid = %d] NativeWindowBuffer::NativeWindowBuffer: created: this = %p anwb = %p\n", gettid(), this, static_cast<ANativeWindowBuffer *>(this));

    ANativeWindowBuffer::handle = nullptr;
    ANativeWindowBuffer::common.incRef = _incRef;
    ANativeWindowBuffer::common.incRef(&common);
    ANativeWindowBuffer::common.decRef = _decRef;
}

NativeWindowBuffer::NativeWindowBuffer(int _width, int _height, int _format, int _usage): fenceFd(-1), mRefCount(0) {
    printf("[tid = %d] NativeWindowBuffer::NativeWindowBuffer = %p anwb = %p\n", gettid(), this, static_cast<ANativeWindowBuffer *>(this));
    ANativeWindowBuffer::width = _width;
    ANativeWindowBuffer::height = _height;
    ANativeWindowBuffer::format = _format;
    ANativeWindowBuffer::usage = _usage;

    ANativeWindowBuffer::common.incRef = _incRef;
    ANativeWindowBuffer::common.incRef(&common);
    ANativeWindowBuffer::common.decRef = _decRef;

    if(!gr_module) {
        int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **) &gr_module);
        if (err != 0)
            printf("[tid = %d] NativeWindowBuffer::NativeWindowBuffer: failed to get gralloc module: %s\n", gettid(), strerror(-err));
    }

    if(!gr_dev) {
        int err = gralloc_open((const hw_module_t *) gr_module, &gr_dev);
        if (err) {
            printf("[tid = %d] NativeWindowBuffer::NativeWindowBuffer: failed to open gralloc: (%s)\n", gettid(), strerror(-err));
        } else {
            printf("[tid = %d] NativeWindowBuffer::NativeWindowBuffer: gralloc_open %p status=%s\n", gettid(), gr_dev, strerror(-err));
        }
    }

    int err = gr_dev->alloc(gr_dev, width, height, format, usage, &handle, &stride);
    if(err) {
        printf("[tid = %d] NativeWindowBuffer::NativeWindowBuffer: failed to gr_dev->alloc: (%s)\n", gettid(), strerror(-err));
    } else {
        printf("[tid = %d] NativeWindowBuffer::NativeWindowBuffer: created: this = %p stride = %d handle: version = %d numFds = %d numInts = %d\n",
                gettid(), this, stride, handle->version, handle->numFds, handle->numInts);
    }
}

NativeWindowBuffer::~NativeWindowBuffer() {
    printf("[tid = %d] NativeWindowBuffer::~NativeWindowBuffer this = %p anwb = %p ownGrMemory = %d gr_dev = %p handle = %p\n",
           gettid(), this, static_cast<ANativeWindowBuffer *>(this), ownGrMemory, gr_dev, handle);
    if(ownGrMemory) {
        if(gr_dev)
            gr_dev->free(gr_dev, handle);
    } else if(handle) {
        native_handle_close(handle);
        ::free(const_cast<native_handle*>(handle));
    }

    if(fenceFd != -1)
        close(fenceFd);
}

int NativeWindowBuffer::closeGrDev() {
    int result = 0;
    if(gr_dev) {
        result = gralloc_close(gr_dev);
        if(result == 0) {
            gr_dev = nullptr;
        }
    }
    return result;
}

void NativeWindowBuffer::_incRef(struct android_native_base_t * base) {
    ANativeWindowBuffer * anwb = reinterpret_cast<ANativeWindowBuffer *>(base);
    NativeWindowBuffer * self = static_cast<NativeWindowBuffer *>(anwb);
    __sync_fetch_and_add(&self->mRefCount, 1);
    printf("[tid = %d] NativeWindowBuffer::_incRef self = %p anwb = %p mRefCount = %d\n", gettid(), self, anwb, self->mRefCount);
}

void NativeWindowBuffer::_decRef(struct android_native_base_t * base) {
    ANativeWindowBuffer * anwb = reinterpret_cast<ANativeWindowBuffer *>(base);
    NativeWindowBuffer * self = static_cast<NativeWindowBuffer *>(anwb);
    printf("[tid = %d] NativeWindowBuffer::_decRef self = %p anwb = %p mRerfCount = %d\n", gettid(), self, anwb, self->mRefCount - 1);
    if (__sync_fetch_and_sub(&self->mRefCount, 1) == 1) {
        delete self;
    }
}

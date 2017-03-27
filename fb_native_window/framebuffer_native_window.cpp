#include <hardware/gralloc.h>
#include <list>
#include <map>
#include <system/window.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <string>
#include "util.h"
#include <time.h>
#include <stdlib.h>
#include "native_window_buffer.h"
#include "framebuffer_native_window.h"
#include "gralloc_stuff.h"
#include "fps.h"
#include <sstream>
#include <system/window.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <sys/time.h>
#include <unistd.h>

#include <sync/sync.h>

/*
static void hwc_invalidate(const struct hwc_procs * procs) {
    printf("hwc_invalidate procs = %p\n", procs);
}
static void hwc_vsync(const struct hwc_procs * procs, int disp, int64_t ts) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    printf("%ld [pid = %d tid = %d] hwc_vsync procs = %p disp = %d ts = %lld p\n", start_time.tv_sec * 1000 + start_time.tv_usec / 1000, getpid(), gettid(), procs, disp, ts);
}

static void hwc_hotplug(const struct hwc_procs *procs, int disp, int conn) { 
    printf("hwc_hotplug procs = %p disp = %d conn = %d p\n", procs, disp, conn);
}

struct hwc_procs hprocs = {
    .invalidate = hwc_invalidate,
    .vsync = hwc_vsync,
    .hotplug = hwc_hotplug,
};
*/

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

FramebufferNativeWindow::FramebufferNativeWindow(): mRefCount(0), mBufferCount(2), mWidth(0), mHeight(0), mFormat(0), mUsage(0) {
    ANativeWindow::common.incRef = FramebufferNativeWindow::_incRef;
    common.incRef(&common);
    ANativeWindow::common.decRef = FramebufferNativeWindow::_decRef;

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

/*
    hw_module_t * hwc_module;
    int err = hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t **) &hwc_module);
    if(err != 0) printf("hw_get_module error: %d: %s\n", err, strerror(err));

    err = hwc_open_1(hwc_module, &hwcdevice);
    if(err != 0) printf("hw_get_module error: %d: %s\n", err, strerror(err));

    hwcdevice->registerProcs(hwcdevice, &hprocs);
    hwcdevice->eventControl(hwcdevice, HWC_DISPLAY_PRIMARY, HWC_EVENT_VSYNC, 1);
*/

    const hw_module_t * gr_module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **) &gr_module);
    if (err != 0) {
        printf("FramebufferNativeWindow::FramebufferNativeWindow: failed to get gralloc module: %s\n", strerror(-err));
        exit(1);
    }

    err = framebuffer_open((hw_module_t *) gr_module, &mFbDev);
//  err = gr_module->methods->open(gr_module, GRALLOC_HARDWARE_FB0, (struct hw_device_t**)&mFbDev);
    if (err) {
        printf("FramebufferNativeWindow::FramebufferNativeWindow: failed to open fbDev: (%s)\n", strerror(-err));
        exit(1);
    }

//    mBufferCount = mFbDev->numFramebuffers;
    mWidth = mFbDev->width;
    mHeight = mFbDev->height;
    mFormat = mFbDev->format;
    mUsage = GRALLOC_USAGE_HW_FB |
        GRALLOC_USAGE_HW_COMPOSER |
        GRALLOC_USAGE_HW_RENDER;

//      mFbDev->enableScreen(mFbDev, 0);

    printf("FramebufferNativeWindow::FramebufferNativeWindow: framebuffer opened: status=(%s) width = %d height = %d format=0x%x numFramebuffers = %d\n",
            strerror(-err), mFbDev->width, mFbDev->height, mFbDev->format, mFbDev->numFramebuffers);
}

FramebufferNativeWindow::~FramebufferNativeWindow() {
    printf("FramebufferNativeWindow::~FramebufferNativeWindow this = %p\n", this);
    framebuffer_close(mFbDev);
}

int FramebufferNativeWindow::width() {
    return mWidth;
}

int FramebufferNativeWindow::height() {
    return mHeight;
}

void FramebufferNativeWindow::resize(int width, int height) {
    if(mWidth == width && mHeight == height)
        return;

    printf("FramebufferNativeWindow::resize width = %d height = %d\n", width, height);
    freeBuffers();

    mWidth = width;
    mHeight = height;
}

void FramebufferNativeWindow::_incRef(struct android_native_base_t * base) {
    ANativeWindow * anw = reinterpret_cast<ANativeWindow*>(base);
    FramebufferNativeWindow * self = static_cast<FramebufferNativeWindow *>(anw);
    __sync_fetch_and_add(&self->mRefCount, 1);
    printf("[tid = %d] FramebufferNativeWindow::_incRef self = %p mRefCount = %d\n", gettid(), self, self->mRefCount);
}

void FramebufferNativeWindow::_decRef(struct android_native_base_t * base) {
    ANativeWindow * anw = reinterpret_cast<ANativeWindow*>(base);
    FramebufferNativeWindow * self = static_cast<FramebufferNativeWindow *>(anw);
    printf("[tid = %d] FramebufferNativeWindow::_decRef self = %p mRerfCount = %d\n", gettid(), self, self->mRefCount - 1);
    if (__sync_fetch_and_sub(&self->mRefCount, 1) == 1) {
        delete self;
    }
}

int FramebufferNativeWindow::_cancelBuffer(ANativeWindow * window, ANativeWindowBuffer * buffer, int fenceFd) {
    printf("FramebufferNativeWindow::_cancelBuffer window = %p buffer = %p fence = %d not implemented\n", window, buffer, fenceFd);
    return 0;
}

int FramebufferNativeWindow::_query(const ANativeWindow * window, int what, int* value) {
    printf("[tid = %d] FramebufferNativeWindow::_query window = %p what = %s returned value = ", gettid(), window, native_query_operation(what).c_str());
    const FramebufferNativeWindow * self = static_cast<const FramebufferNativeWindow*>(window);
    switch (what) {
//        case NATIVE_WINDOW_WIDTH:
        case NATIVE_WINDOW_DEFAULT_WIDTH:
            *value = self->mWidth;
            break;

//        case NATIVE_WINDOW_HEIGHT:
        case NATIVE_WINDOW_DEFAULT_HEIGHT:
            *value = self->mHeight;
            break;

        case NATIVE_WINDOW_FORMAT:
            *value = self->mFormat;
            break;

        case NATIVE_WINDOW_TRANSFORM_HINT:
            *value = 0;
            break;
        case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
            *value = 1;
            break;
        case NATIVE_WINDOW_DEFAULT_DATASPACE:
            *value = HAL_DATASPACE_UNKNOWN;
            break;

        default:
            *value = 0;
            printf(" -1\nFramebufferNativeWindow::_query window = %p what = %s not implemented query\n", window, native_query_operation(what).c_str());
            return -1;
    }
    printf("%d\n", *value);
    return 0;
}

int FramebufferNativeWindow::_perform(ANativeWindow * window, int operation, ... ) {
    va_list args;
    va_start(args, operation);
    FramebufferNativeWindow * self = static_cast<FramebufferNativeWindow *>(window);
    printf("[tid = %d] FramebufferNativeWindow::_perform self = %p window = %p operation = %s", gettid(), self, window, native_window_operation(operation).c_str());
    
    switch(operation) {
        case NATIVE_WINDOW_API_CONNECT:
        case NATIVE_WINDOW_API_DISCONNECT:
            printf("\n");
            break;
        
        case NATIVE_WINDOW_SET_USAGE: {
            int usage = va_arg(args, int);
            printf(" usage = 0x%x mUsage = 0x%x\n", usage, self->mUsage);
            if(self->mUsage != usage) {
                self->freeBuffers();
                self->mUsage |= usage;
            }
            break;
        }

        case NATIVE_WINDOW_SET_BUFFER_COUNT: {
            int buffer_count = va_arg(args, int);
            printf(" new buffer count = %d prev buffer_count = %d\n", buffer_count, self->mBufferCount);
            if(self->mBufferCount != buffer_count) {
                self->freeBuffers();
                self->mBufferCount = buffer_count;
            }
            break;
        }

        case NATIVE_WINDOW_SET_BUFFERS_FORMAT: {
            int buffer_format = va_arg(args, int);
            printf(" new format = 0x%x prev format = 0x%x\n", buffer_format, self->mFormat);
            if(buffer_format == 0) { // cleanup in android::egl_surface_t::~egl_surface_t
                printf(" new format = 0, do nothing\n");
                break;
            }
            if(self->mFormat != buffer_format) {
                self->freeBuffers();
                self->mFormat = buffer_format;
            }
            break;
        }

        case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS: {
            int width = va_arg(args, int);
            int height = va_arg(args, int);
            printf(" width = %d height = %d old width = %d old height = %d \n", width, height, self->mWidth, self->mHeight);
            self->resize(width, height);
            break;
        }

        case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM: {
            int transform = va_arg(args, int);
            printf(" transform = 0x%x \n", transform);
            if(transform != 0) printf("FramebufferNativeWindow::_perform operation = %s transform = %d not implemented", native_window_operation(operation).c_str(), transform);
            break;
        }

        default: {
            va_end(args);
            printf(": \033[0;31mnot implemented\033[0m\n");
            return 0;
        }
    }
    va_end(args);
    return 0;
}

int FramebufferNativeWindow::_setSwapInterval(ANativeWindow * window, int interval) {
    printf("FramebufferNativeWindow::_setSwapInterval window = %p interval = %d\n", window, interval);
    return 0;
}

int FramebufferNativeWindow::_dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer, int* fenceFd) {
    printf("FramebufferNativeWindow::_dequeueBuffer window = %p\n", window);
    FramebufferNativeWindow * self = static_cast<FramebufferNativeWindow *>(window);

    if(self->mBuffers.empty())
    self->allocBuffers();

    *buffer = *(self->mNextBackBuffer++);

    if(self->mNextBackBuffer == self->mBuffers.end())
        self->mNextBackBuffer = self->mBuffers.begin();

    *fenceFd = -1;

    return 0;
}

int FramebufferNativeWindow::_queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd) {
    FPS fps;
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    printf("%ld [tid = %d] FramebufferNativeWindow::_queueBuffer enter window = %p buffer = %p fence = %d\n", start_time.tv_sec * 1000 + start_time.tv_usec / 1000, gettid(),  window, buffer, fenceFd);

    if (fenceFd >= 0) {
        struct timeval start_time, end_time;
        gettimeofday(&start_time, NULL);
        sync_wait(fenceFd, -1);
        gettimeofday(&end_time, NULL);
        printf("FramebufferNativeWindow::_queueBuffer: sync time = %ld millisecs\n\n", (end_time.tv_sec * 1000000 + end_time.tv_usec -    start_time.tv_sec * 1000000 - start_time.tv_usec) / 1000);
        close(fenceFd);
    }

    FramebufferNativeWindow * self = static_cast<FramebufferNativeWindow *>(window);
    
//  API way:
//  self->mFbDev->post(self->mFbDev, buffer->handle);

//  direct for hisi3660:
    gralloc_specific_fb_post(self->mFbDev, buffer->handle);

    gettimeofday(&end_time, NULL);
    printf("%ld [pid = %d tid = %d] FramebufferNativeWindow::_queueBuffer: mFbDev->post time = %ld\n", end_time.tv_sec * 1000 + end_time.tv_usec / 1000, getpid(), gettid(), end_time.tv_sec * 1000 + end_time.tv_usec / 1000 -  start_time.tv_sec * 1000 - start_time.tv_usec / 1000);

    return 0;
}

void FramebufferNativeWindow::allocBuffers() {
    printf("FramebufferNativeWindow::allocBuffers\n");
    for(int i = 0; i < mBufferCount; i++) {
        NativeWindowBuffer * buf = new NativeWindowBuffer(mWidth, mHeight, mFormat, mUsage);
        mBuffers.push_back(buf);
    }

    mNextBackBuffer = mBuffers.begin();
}

void FramebufferNativeWindow::freeBuffers() {
    printf("FramebufferNativeWindow::freeBuffers\n");
    for(NativeWindowBuffer * buffer : mBuffers) {
        buffer->common.decRef(&buffer->common);
    }

    mBuffers.clear();
    mNextBackBuffer = mBuffers.end();
}

/*
int FramebufferNativeWindow::_setSwapInterval(ANativeWindow* window, int interval) {
    printf("FramebufferNativeWindow::_setSwapInterval window = %p interval = %d\n", window, interval);
    FramebufferNativeWindow * self = static_cast<FramebufferNativeWindow*>(window);
    int result = self->mFbDev->setSwapInterval(self->mFbDev, interval);
    if(result != 0)
        printf("FramebufferNativeWindow::_setSwapInterval error = %d %s\n", result, strerror(-result));

    return result;
}
*/

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
#include "hwc_native_window.h"
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

HWCNativeWindow::HWCNativeWindow(int width, int height): mRefCount(0), mBufferCount(0), mWidth(width), mHeight(height), mFormat(0), mUsage(0) {
    ANativeWindow::common.incRef = HWCNativeWindow::_incRef;
    common.incRef(&common);
    ANativeWindow::common.decRef = HWCNativeWindow::_decRef;

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

    mBufferCount = 2;
    mWidth = width;
    mHeight = height;
    mFormat = HAL_PIXEL_FORMAT_RGBA_8888; // HAL_PIXEL_FORMAT_RGB_565
    mUsage = GRALLOC_USAGE_HW_COMPOSER; // GRALLOC_USAGE_SW_READ_RARELY

    hw_module_t * hwc_module;
    int err = hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t **) &hwc_module);
    if(err != 0) printf("hw_get_module error: %d: %s\n", err, strerror(err));

    err = hwc_open_1(hwc_module, &hwcdevice);
    if(err != 0) printf("hw_get_module error: %d: %s\n", err, strerror(err));

//    hwcdevice->registerProcs(hwcdevice, &hprocs);

//    hwcdevice->blank(hwcdevice, HWC_DISPLAY_PRIMARY, 0);

    static const uint32_t DISPLAY_ATTRIBUTES[] = {
        HWC_DISPLAY_VSYNC_PERIOD,
        HWC_DISPLAY_WIDTH,
        HWC_DISPLAY_HEIGHT,
        HWC_DISPLAY_DPI_X,
        HWC_DISPLAY_DPI_Y,
        HWC_DISPLAY_NO_ATTRIBUTE,
    };
    #define NUM_DISPLAY_ATTRIBUTES (sizeof(DISPLAY_ATTRIBUTES) / sizeof(DISPLAY_ATTRIBUTES)[0])

    int32_t values[NUM_DISPLAY_ATTRIBUTES - 1];
    memset(values, 0, sizeof(values));

    uint32_t config;
    size_t nconfigs = 1;
    err = hwcdevice->getDisplayConfigs(hwcdevice, HWC_DISPLAY_PRIMARY, &config, &nconfigs);
    if (err != 0) printf("getDisplayConfigs error: %d: %s\n", err, strerror(err));

    err = hwcdevice->getDisplayAttributes(hwcdevice, HWC_DISPLAY_PRIMARY, config, DISPLAY_ATTRIBUTES, values);
    if (err != 0) printf("getDisplayAttributes error: %d: %s\n", err, strerror(err));

    for (size_t i = 0; i < NUM_DISPLAY_ATTRIBUTES - 1; i++) {
        switch (DISPLAY_ATTRIBUTES[i]) {
        case HWC_DISPLAY_VSYNC_PERIOD: printf("hwc primary display vsync period = %d\n", values[i]); break;
        case HWC_DISPLAY_WIDTH: printf("hwc primary display width = %d\n", values[i]); break;
        case HWC_DISPLAY_HEIGHT: printf("hwc primary display height = %d\n", values[i]); break;
        case HWC_DISPLAY_DPI_X: printf("hwc primary display xdpi = %d\n", values[i]); break;
        case HWC_DISPLAY_DPI_Y: printf("hwc primary display ydpi = %d\n", values[i]); break;
        default:printf("hwc primary display unknown display attribute[%zu] %#x: %d", i, DISPLAY_ATTRIBUTES[i], values[i]);
            break;
        }
    }

    size_t size = sizeof(hwc_display_contents_1_t) + 2 * sizeof(hwc_layer_1_t);
    hwc_display_contents_1_t * list = (hwc_display_contents_1_t *) malloc(size);
    mList = (hwc_display_contents_1_t **) malloc(HWC_NUM_DISPLAY_TYPES * sizeof(hwc_display_contents_1_t *));
    const hwc_rect_t r = { 0, 0, mWidth, mHeight};

    mList[0] = list;
    for (int counter = 1; counter < HWC_NUM_DISPLAY_TYPES; counter++)
        mList[counter] = 0;

    fblayer = &list->hwLayers[0];
    memset(fblayer, 0, sizeof(hwc_layer_1_t));
    fblayer->compositionType = HWC_FRAMEBUFFER;
    fblayer->hints = 0;
    fblayer->flags = 0;
    fblayer->handle = 0;
    fblayer->transform = 0;
    fblayer->blending = HWC_BLENDING_NONE;
    fblayer->sourceCrop = r;
    fblayer->displayFrame = r;
    fblayer->visibleRegionScreen.numRects = 1;
    fblayer->visibleRegionScreen.rects = &fblayer->displayFrame;
    fblayer->acquireFenceFd = -1;
    fblayer->releaseFenceFd = -1;
    fblayer = &list->hwLayers[1];
    memset(fblayer, 0, sizeof(hwc_layer_1_t));
    fblayer->compositionType = HWC_FRAMEBUFFER_TARGET;
    fblayer->hints = 0;
    fblayer->flags = 0;
    fblayer->handle = 0;
    fblayer->transform = 0;
    fblayer->blending = HWC_BLENDING_NONE;
    fblayer->sourceCrop = r;
    fblayer->displayFrame = r;
    fblayer->visibleRegionScreen.numRects = 1;
    fblayer->visibleRegionScreen.rects = &fblayer->displayFrame;
    fblayer->acquireFenceFd = -1;
    fblayer->releaseFenceFd = -1;

    list->retireFenceFd = -1;
    list->flags = HWC_GEOMETRY_CHANGED;
    list->numHwLayers = 2;
}

HWCNativeWindow::~HWCNativeWindow() {
    printf("HWCNativeWindow::~HWCNativeWindow this = %p\n", this);
    freeBuffers();
}

int HWCNativeWindow::width() {
    return mWidth;
}

int HWCNativeWindow::height() {
    return mHeight;
}

void HWCNativeWindow::resize(int width, int height) {
    if(mWidth == width && mHeight == height)
        return;

    printf("HWCNativeWindow::resize width = %d height = %d\n", width, height);
    freeBuffers();

    mWidth = width;
    mHeight = height;
}

void HWCNativeWindow::_incRef(struct android_native_base_t * base) {
    ANativeWindow * anw = reinterpret_cast<ANativeWindow*>(base);
    HWCNativeWindow * self = static_cast<HWCNativeWindow *>(anw);
    __sync_fetch_and_add(&self->mRefCount, 1);
    printf("[tid = %d] HWCNativeWindow::_incRef self = %p mRefCount = %d\n", gettid(), self, self->mRefCount);
}

void HWCNativeWindow::_decRef(struct android_native_base_t * base) {
    ANativeWindow * anw = reinterpret_cast<ANativeWindow*>(base);
    HWCNativeWindow * self = static_cast<HWCNativeWindow *>(anw);
    printf("[tid = %d] HWCNativeWindow::_decRef self = %p mRerfCount = %d\n", gettid(), self, self->mRefCount - 1);
    if (__sync_fetch_and_sub(&self->mRefCount, 1) == 1) {
        delete self;
    }
}

int HWCNativeWindow::_cancelBuffer(ANativeWindow * window, ANativeWindowBuffer * buffer, int fenceFd) {
    printf("HWCNativeWindow::_cancelBuffer window = %p buffer = %p fence = %d not implemented\n", window, buffer, fenceFd);
    return 0;
}

int HWCNativeWindow::_query(const ANativeWindow * window, int what, int* value) {
    printf("[tid = %d] HWCNativeWindow::_query window = %p what = %s returned value = ", gettid(), window, native_query_operation(what).c_str());
    const HWCNativeWindow * self = static_cast<const HWCNativeWindow*>(window);
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
            printf(" -1\nHWCNativeWindow::_query window = %p what = %s not implemented query\n", window, native_query_operation(what).c_str());
            return -1;
    }
    printf("%d\n", *value);
    return 0;
}

int HWCNativeWindow::_perform(ANativeWindow * window, int operation, ... ) {
    va_list args;
    va_start(args, operation);
    HWCNativeWindow * self = static_cast<HWCNativeWindow *>(window);
    printf("[tid = %d] HWCNativeWindow::_perform self = %p window = %p operation = %s", gettid(), self, window, native_window_operation(operation).c_str());
    
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
            if(transform != 0) printf("HWCNativeWindow::_perform operation = %s transform = %d not implemented", native_window_operation(operation).c_str(), transform);
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

int HWCNativeWindow::_setSwapInterval(ANativeWindow * window, int interval) {
    printf("HWCNativeWindow::_setSwapInterval window = %p interval = %d\n", window, interval);
    return 0;
}

int HWCNativeWindow::_dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer, int* fenceFd) {
    printf("HWCNativeWindow::_dequeueBuffer window = %p\n", window);
    HWCNativeWindow * self = static_cast<HWCNativeWindow *>(window);

    if(self->mBuffers.empty())
    self->allocBuffers();

    NativeWindowBuffer * native_buffer = (*(self->mNextBackBuffer++));
    *buffer = static_cast<ANativeWindowBuffer *>(native_buffer);

    if(self->mNextBackBuffer == self->mBuffers.end())
        self->mNextBackBuffer = self->mBuffers.begin();

    *fenceFd = dup(native_buffer->fenceFd);
    close(native_buffer->fenceFd);
    native_buffer->fenceFd = -1;

    return 0;
}

int HWCNativeWindow::_queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd) {
    HWCNativeWindow * self = static_cast<HWCNativeWindow *>(window);

   struct timeval start_time;
   gettimeofday(&start_time, NULL);
   printf("%ld [tid = %d] HWCNativeWindow::_queueBuffer enter window = %p buffer = %p fence = %d\n",start_time.tv_sec * 1000 + start_time.tv_usec / 1000, gettid(),  window, buffer, fenceFd);

    static_cast<NativeWindowBuffer *>(buffer)->fenceFd = fenceFd;

    int oldretire = self->mList[0]->retireFenceFd;
    self->mList[0]->retireFenceFd = -1;
    self->fblayer->handle = buffer->handle;
    self->fblayer->acquireFenceFd = static_cast<NativeWindowBuffer *>(buffer)->fenceFd;
    self->fblayer->releaseFenceFd = -1;
    int err = self->hwcdevice->prepare(self->hwcdevice, HWC_NUM_DISPLAY_TYPES, self->mList);
    if(err != 0)
        printf("HWCNativeWindow::_queueBuffer hwcdevice->prepare error %d %s\n", err, strerror(errno));

    err = self->hwcdevice->set(self->hwcdevice, HWC_NUM_DISPLAY_TYPES, self->mList);
    if(err != 0)
        printf("HWCNativeWindow::_queueBuffer hwcdevice->prepare error %d %s\n", err, strerror(errno));

    static_cast<NativeWindowBuffer *>(buffer)->fenceFd = self->fblayer->releaseFenceFd;

    if (oldretire != -1) {
        struct timeval start_time, end_time;
        gettimeofday(&start_time, NULL);

        sync_wait(oldretire, -1);
        close(oldretire);

        gettimeofday(&end_time, NULL);
        printf("[pid = %d] HWCNativeWindow::_queueBuffer: fd = %d sync time = %ld millisecs\n",getpid(), oldretire, (end_time.tv_sec * 1000000 + end_time.tv_usec -  start_time.tv_sec * 1000000 - start_time.tv_usec) / 1000);
    }

    static int fps_time = 0;
    static int fps_counter = 0;

    if(fps_counter == 30) {
        struct timeval start_time;
        gettimeofday(&start_time, NULL);
        int new_fps_time = start_time.tv_sec * 1000 + start_time.tv_usec / 1000;
        printf("HWCNativeWindow::_queueBuffer fps = %3f\n", fps_counter * 1000.0 / (new_fps_time - fps_time));
        fps_counter = 0;
        fps_time = new_fps_time;
    }
    fps_counter++;

    gettimeofday(&start_time, NULL);
    printf("%ld [tid = %d] HWCNativeWindow::_queueBuffer exit window = %p buffer = %p fence = %d\n",start_time.tv_sec * 1000 + start_time.tv_usec / 1000, gettid(),  window, buffer, fenceFd);

    return 0;
}

void HWCNativeWindow::allocBuffers() {
    printf("HWCNativeWindow::allocBuffers\n");
    for(int i = 0; i < mBufferCount; i++) {
        NativeWindowBuffer * buf = new NativeWindowBuffer(mWidth, mHeight, mFormat, mUsage);
        mBuffers.push_back(buf);
    }

    mNextBackBuffer = mBuffers.begin();
}

void HWCNativeWindow::freeBuffers() {
    printf("HWCNativeWindow::freeBuffers\n");
    for(NativeWindowBuffer * buffer : mBuffers) {
        buffer->common.decRef(&buffer->common);
    }

    mBuffers.clear();
    mNextBackBuffer = mBuffers.end();
}

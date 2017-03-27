#pragma once

#include <hardware/gralloc.h>
#include <hardware/hwcomposer.h>
#include <list>
#include <map>
#include <system/window.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <string>
#include "util.h"
#include <time.h>

class NativeWindowBuffer;

class HWCNativeWindow : public ANativeWindow {
public:
    HWCNativeWindow(int width, int height);

    int width();
    int height();

    void resize(int width, int height);

private:
    virtual ~HWCNativeWindow();

    static void _incRef(struct android_native_base_t * base);
    static void _decRef(struct android_native_base_t * base);
    static int _cancelBuffer(ANativeWindow * window, ANativeWindowBuffer * buffer, int fenceFd);
    static int _query(const ANativeWindow * window, int what, int* value);
    static int _perform(ANativeWindow * window, int operation, ... );
    static int _setSwapInterval(ANativeWindow * window, int interval);
    static int _dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer, int* fenceFd);
    static int _queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd);

    void allocBuffers();
    void freeBuffers();

    volatile int mRefCount;

    int mBufferCount;
    int mWidth;
    int mHeight;
    int mFormat;
    int mUsage;

    std::list<NativeWindowBuffer *> mBuffers;
    std::list<NativeWindowBuffer *>::iterator mNextBackBuffer;

    hwc_composer_device_1_t * hwcdevice;
    hwc_display_contents_1_t ** mList;
    hwc_layer_1_t * fblayer;
};

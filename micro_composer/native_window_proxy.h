#pragma once

#include <binder/IBinder.h>
#include <system/window.h>

class NativeWindowBuffer;

class NativeWindowProxy: public ANativeWindow {
public:
    NativeWindowProxy();

private:
    virtual ~NativeWindowProxy();

    static void _incRef(struct android_native_base_t * base);
    static void _decRef(struct android_native_base_t * base);
    static int _cancelBuffer(ANativeWindow * window, ANativeWindowBuffer * buffer, int fenceFd);
    static int _query(const ANativeWindow * window, int what, int* value);
    static int _perform(ANativeWindow * window, int operation, ... );
    static int _setSwapInterval(ANativeWindow * window, int interval);
    static int _dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer, int* fenceFd);
    static int _queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd);

    volatile int mRefCount;

    android::sp<android::IBinder> native_window_binder;
};

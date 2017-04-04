#pragma once

#include "utils.h"
#include <binder/Binder.h>
#include <system/window.h>

class NativeWindowStub: public android::BBinder {
public:
    NativeWindowStub(ANativeWindow * native_window): native_window(native_window) {
        printf("NativeWindowStub::NativeWindowStub(): this = %p native_window = %p\n", this, native_window);
    }

    virtual ~NativeWindowStub() {
        printf("NativeWindowStub::~NativeWindowStub()\n");
        native_window->common.decRef(&native_window->common);
    }

    android::status_t onTransact(uint32_t code, const android::Parcel& data, android::Parcel * reply, uint32_t flags = 0) override;

private:
    ANativeWindow * native_window;
};
